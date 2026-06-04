/*
 * SD_CARD_DRIVER.c
 *
 * Layer  : HAL
 * Target : ATmega2560 @ 16MHz
 * Device : SD card, SPI block-level access.
 *
 * Init flow (SPI mode):
 *   1) Hold CS high and send >74 clocks so the card powers into native mode.
 *   2) CMD0  (GO_IDLE_STATE) with CS low -> card enters SPI idle (R1 = 0x01).
 *   3) CMD8  (SEND_IF_COND) -> distinguishes v2 cards / checks voltage.
 *   4) ACMD41 (APP_SEND_OP_COND) repeatedly until the card leaves idle (R1=0).
 *   Then it is ready for CMD17 (read block) and CMD24 (write block).
 *
 * Author : (Mohamed Eid)
 */

#include "SD_CARD_DRIVER.h"  /* Public types, enums and prototypes */

/*------------------------------------------------------------------------
 * SD command opcodes we use (the SPI command byte is 0x40 | index).
 *----------------------------------------------------------------------*/
#define CMD0    0     /* GO_IDLE_STATE : software reset into SPI idle */
#define CMD8    8     /* SEND_IF_COND  : voltage check / v2 detection */
#define CMD17   17    /* READ_SINGLE_BLOCK */
#define CMD24   24    /* WRITE_SINGLE_BLOCK */
#define CMD55   55    /* APP_CMD : prefix that precedes any ACMD */
#define ACMD41  41    /* SD_SEND_OP_COND : start the init / leave idle */

/*------------------------------------------------------------------------
 * Saved configuration (the CS pin).
 *----------------------------------------------------------------------*/
static SD_Config g_sd;         /* Private copy of the active configuration */

/*------------------------------------------------------------------------
 * DEFAULT configuration used when SD_init() receives NULL.
 * CS on PB0 (the SPI SS pin on the 2560).
 *----------------------------------------------------------------------*/
const SD_Config SD_DEFAULT_CONFIG = {
	.m_cs_port = DIO_PORTB,    /* CS on PORTB ... */
	.m_cs_pin  = DIO_PIN0      /* ... pin 0 */
};

/*------------------------------------------------------------------------
 * Low-level CS helpers (active-low chip select).
 *----------------------------------------------------------------------*/
static void sd_cs_low(void)  { DIO_writePin(g_sd.m_cs_port, g_sd.m_cs_pin, DIO_LOW);  }   /* Select the card */
static void sd_cs_high(void) { DIO_writePin(g_sd.m_cs_port, g_sd.m_cs_pin, DIO_HIGH); }   /* Deselect the card */

/*------------------------------------------------------------------------
 * Exchange one byte on SPI (thin wrapper for readability).
 *----------------------------------------------------------------------*/
static uint8_t sd_xfer(uint8_t b)
{
	return SPI_transfer(b);    /* Full-duplex: send b, get the card's byte back */
}

/*------------------------------------------------------------------------
 * Send a command frame and return the R1 response byte.
 * Frame = [0x40|cmd][arg 31..0][CRC]. CRC only matters for CMD0/CMD8.
 *----------------------------------------------------------------------*/
static uint8_t sd_command(uint8_t cmd, uint32_t arg, uint8_t crc)
{
	sd_xfer(0x40 | cmd);                 /* Command byte: start bits + index */
	sd_xfer((uint8_t)(arg >> 24));       /* Argument, most-significant byte first */
	sd_xfer((uint8_t)(arg >> 16));
	sd_xfer((uint8_t)(arg >> 8));
	sd_xfer((uint8_t)(arg));
	sd_xfer(crc);                        /* CRC (valid CRCs only required for CMD0/CMD8) */

	/* The R1 response arrives within 8 byte-times; the top bit is 0 when valid. */
	uint8_t r1;
	for(uint8_t i = 0; i < 8; i++)       /* Poll up to 8 times for a valid response */
	{
		r1 = sd_xfer(0xFF);              /* Clock out 0xFF to read the card's byte */
		if((r1 & 0x80) == 0) break;      /* Bit7 clear -> this is the R1 response */
	}
	return r1;                           /* Return whatever we got (0x01 = idle, 0x00 = ready) */
}

/*========================================================================
 * SD_init : bring the card up in SPI mode.
 *======================================================================*/
SD_Status SD_init(const SD_Config* config)
{
	if(config == 0)              /* NULL-safe: use the default CS pin */
	{
		config = &SD_DEFAULT_CONFIG;
	}
	g_sd = *config;              /* Keep a private copy of the configuration */

	DIO_setDirection(g_sd.m_cs_port, g_sd.m_cs_pin, DIO_OUTPUT);   /* CS is an output */
	sd_cs_high();                /* Start deselected */

	/* Step 1: >= 74 dummy clocks with CS high so the card self-powers up. */
	for(uint8_t i = 0; i < 10; i++)      /* 10 bytes = 80 clocks (> 74) */
	{
		sd_xfer(0xFF);
	}

	/* Step 2: CMD0 with CS low -> expect R1 = 0x01 (idle). */
	sd_cs_low();
	uint8_t r1 = sd_command(CMD0, 0x00000000, 0x95);   /* 0x95 is the correct CRC for CMD0 */
	if(r1 != 0x01)                                      /* Must be "idle" */
	{
		sd_cs_high();
		return SD_ERR_INIT;                             /* Card did not reset into SPI idle */
	}

	/* Step 3: CMD8 -> voltage/version check (0x1AA pattern, CRC 0x87). */
	sd_command(CMD8, 0x000001AA, 0x87);                 /* Response details ignored for simplicity */
	/* (A full driver would read 4 trailing bytes here; we proceed for both v1/v2.) */
	for(uint8_t i = 0; i < 4; i++) sd_xfer(0xFF);       /* Flush CMD8's 32-bit trailing data */

	/* Step 4: ACMD41 loop until the card leaves idle (R1 becomes 0x00). */
	uint16_t tries = 0;                                 /* Timeout counter */
	do {
		sd_command(CMD55, 0x00000000, 0xFF);            /* APP_CMD prefix before any ACMD */
		r1 = sd_command(ACMD41, 0x40000000, 0xFF);      /* 0x40000000 = HCS bit (support SDHC) */
		if(++tries > 2000)                              /* Bail out if it never gets ready */
		{
			sd_cs_high();
			return SD_ERR_TIMEOUT;
		}
	} while(r1 != 0x00);                                /* 0x00 means initialisation complete */

	sd_cs_high();                /* Deselect; the card is now ready for block I/O */
	sd_xfer(0xFF);               /* One extra clock byte to release the bus cleanly */
	return SD_OK;
}

/*========================================================================
 * SD_readBlock : read one 512-byte block (CMD17).
 *======================================================================*/
SD_Status SD_readBlock(uint32_t block_number, uint8_t* buffer)
{
	if(buffer == 0) return SD_ERR_PARAM;     /* Reject a NULL destination */

	sd_cs_low();                             /* Select the card */

	/* Byte addressing for standard-capacity cards is block*512, but SDHC/SDXC
	 * are block-addressed. Modern cards are SDHC, so we pass the block number. */
	uint8_t r1 = sd_command(CMD17, block_number, 0xFF);   /* READ_SINGLE_BLOCK */
	if(r1 != 0x00)                                        /* Must accept the command */
	{
		sd_cs_high();
		return SD_ERR_RESPONSE;
	}

	/* Wait for the data start token 0xFE. */
	uint16_t tries = 0;
	uint8_t token;
	do {
		token = sd_xfer(0xFF);               /* Clock until the card sends the start token */
		if(++tries > 10000)                  /* Guard against a stuck card */
		{
			sd_cs_high();
			return SD_ERR_TIMEOUT;
		}
	} while(token != 0xFE);                  /* 0xFE marks the start of the 512 data bytes */

	for(uint16_t i = 0; i < SD_BLOCK_SIZE; i++)   /* Read the 512 payload bytes */
	{
		buffer[i] = sd_xfer(0xFF);
	}
	sd_xfer(0xFF);                           /* Discard the 2-byte CRC ... */
	sd_xfer(0xFF);                           /* ... (CRC is ignored in SPI mode) */

	sd_cs_high();                            /* Deselect */
	sd_xfer(0xFF);                           /* Trailing clock to release the bus */
	return SD_OK;
}

/*========================================================================
 * SD_writeBlock : write one 512-byte block (CMD24).
 *======================================================================*/
SD_Status SD_writeBlock(uint32_t block_number, const uint8_t* buffer)
{
	if(buffer == 0) return SD_ERR_PARAM;     /* Reject a NULL source */

	sd_cs_low();                             /* Select the card */

	uint8_t r1 = sd_command(CMD24, block_number, 0xFF);   /* WRITE_SINGLE_BLOCK */
	if(r1 != 0x00)                                        /* Must accept the command */
	{
		sd_cs_high();
		return SD_ERR_RESPONSE;
	}

	sd_xfer(0xFF);                           /* One gap byte before the data packet */
	sd_xfer(0xFE);                           /* Send the data start token */

	for(uint16_t i = 0; i < SD_BLOCK_SIZE; i++)   /* Send the 512 payload bytes */
	{
		sd_xfer(buffer[i]);
	}
	sd_xfer(0xFF);                           /* Dummy CRC byte 1 (ignored in SPI mode) */
	sd_xfer(0xFF);                           /* Dummy CRC byte 2 */

	/* The card replies with a data-response token: bits[3:1]=010 means accepted. */
	uint8_t resp = sd_xfer(0xFF);
	if((resp & 0x1F) != 0x05)                /* 0x05 = data accepted */
	{
		sd_cs_high();
		return SD_ERR_RESPONSE;
	}

	/* Wait while the card is busy programming (it holds MISO low = 0x00). */
	uint16_t tries = 0;
	while(sd_xfer(0xFF) == 0x00)             /* Non-zero byte means programming finished */
	{
		if(++tries > 30000)                  /* Generous timeout for the write */
		{
			sd_cs_high();
			return SD_ERR_TIMEOUT;
		}
	}

	sd_cs_high();                            /* Deselect */
	sd_xfer(0xFF);                           /* Trailing clock to release the bus */
	return SD_OK;
}
