/*
 * SPI_DRIVER.c
 *
 * Layer  : MCAL
 * Target : ATmega2560 @ 16MHz
 * Purpose: SPI master driver implementation, adapted from the author's code.
 *          Same SPCR/SPSR/SPDR registers as the ATmega32; the only change is
 *          the PORTB pin numbers (SS=PB0, SCK=PB1, MOSI=PB2, MISO=PB3).
 *
 * Author : (Mohamed Eid)
 */

#include "SPI_DRIVER.h"       /* Public types, enums and prototypes */

static uint8_t g_mode = 0;                          /* 0 = polling, 1 = interrupt (cached) */
static void (*volatile spi_callback)(uint8_t) = 0;  /* Interrupt-mode completion callback */

/*------------------------------------------------------------------------
 * DEFAULT configuration used when SPI_init() receives NULL.
 *----------------------------------------------------------------------*/
const SPI_CONFIG SPI_DEFAULT_CONFIG = {
	.m_clock_mode = SPI_CLOCK_MODE_0,   /* Mode 0 (CPOL=0, CPHA=0) */
	.m_rate       = SPI_RATE_64,        /* F_CPU/64 - safe, reliable default */
	.m_mode       = SPI_MODE_POLLING,   /* Polling completion */
	.callback     = 0                   /* No callback by default */
};

/*------------------------------------------------------------------------
 * Helper: apply the rate enum to SPR1:SPR0 (SPCR) and SPI2X (SPSR).
 *----------------------------------------------------------------------*/
static void _set_rate(SPI_RATE rate)
{
	SPCR = (SPCR & 0xFC) | (rate & 0x03);            /* Low 2 bits of 'rate' -> SPR1:SPR0 */
	SPSR = (SPSR & 0xFE) | ((rate & 0x04) >> 2);     /* Bit 2 of 'rate' -> SPI2X (double speed) */
}

/*------------------------------------------------------------------------
 * Helper: apply the clock mode to CPOL:CPHA (bits 3:2 of SPCR).
 *----------------------------------------------------------------------*/
static void _set_clock_mode(SPI_CLOCK_MODE mode)
{
	SPCR = (SPCR & 0xF3) | ((mode & 0x03) << 2);     /* Place CPOL/CPHA into bits 3:2 */
}

/*========================================================================
 * SPI_init : configure the MCU as an SPI master.
 *======================================================================*/
void SPI_init(const SPI_CONFIG* config)
{
	if(config == NULL)               /* NULL-safe: use the default config if none given */
	{
		config = &SPI_DEFAULT_CONFIG;
	}

	/* Pin directions: MOSI, SCK, SS are outputs; MISO is an input. */
	SET_BIT(DDRB, SPI_MOSI_PIN);     /* MOSI output */
	SET_BIT(DDRB, SPI_SCK_PIN);      /* SCK output */
	SET_BIT(DDRB, SPI_SS_PIN);       /* SS output (we control it manually) */
	CLEAR_BIT(DDRB, SPI_MISO_PIN);   /* MISO input */
	SET_BIT(PORTB, SPI_SS_PIN);      /* Idle SS HIGH (no slave selected yet) */

	/* Enable SPI and select master role. */
	SET_BIT(SPCR, SPE);              /* SPE = 1 turns the SPI hardware on */
	SET_BIT(SPCR, MSTR);             /* MSTR = 1 makes this device the master */

	_set_rate(config->m_rate);             /* Program the SCK divider */
	_set_clock_mode(config->m_clock_mode); /* Program CPOL/CPHA */

	/* Choose polling vs interrupt completion. */
	switch(config->m_mode)
	{
		case SPI_MODE_POLLING:               /* Polling completion */
			g_mode = 0;                      /* Remember we are polling */
			break;
		case SPI_MODE_INTERRUPT:             /* Interrupt completion */
			SET_BIT(SPCR, SPIE);             /* Enable the SPI transfer-complete interrupt */
			sei();                           /* Global interrupt enable */
			g_mode = 1;                      /* Remember we are interrupt-driven */
			spi_callback = config->callback; /* Save the completion callback */
			break;
	}
}

/*========================================================================
 * SPI_transfer : send one byte and return the byte clocked back in.
 *======================================================================*/
uint8_t SPI_transfer(uint8_t data)
{
	SPDR = data;                          /* Writing SPDR starts the 8-bit exchange */
	if(g_mode == 0)                       /* Polling mode: wait for completion here */
	{
		while(!CHECK_BIT(SPSR, SPIF));    /* Spin until SPIF (transfer complete) is set */
		return SPDR;                      /* Reading SPDR returns the received byte (and clears SPIF) */
	}
	return 0;                             /* Interrupt mode: result is delivered in the ISR callback */
}

/*========================================================================
 * SPI_select / SPI_deselect : manual control of the Slave-Select line.
 *======================================================================*/
void SPI_select(void)
{
	CLEAR_BIT(PORTB, SPI_SS_PIN);         /* SS LOW selects the slave (active-low) */
}
void SPI_deselect(void)
{
	SET_BIT(PORTB, SPI_SS_PIN);           /* SS HIGH releases the slave */
}

/*========================================================================
 * ISR - SPI transfer complete: forward the received byte to the callback.
 *======================================================================*/
ISR(SPI_STC_vect)
{
	uint8_t received = SPDR;              /* Read the byte clocked in (also clears SPIF) */
	if(spi_callback != 0)                 /* If a callback is registered ... */
	{
		spi_callback(received);           /* ... hand it the received byte */
	}
}
