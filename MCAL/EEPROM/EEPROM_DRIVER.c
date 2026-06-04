/*
 * EEPROM_DRIVER.c
 *
 * Layer  : MCAL
 * Target : ATmega2560 @ 16MHz
 * Purpose: Implementation of the internal EEPROM driver.
 *
 * Hardware write sequence (must be followed exactly):
 *   1) Wait until any previous write finishes  (EEPE bit clears).
 *   2) Wait until any Flash write finishes      (SPMEN bit clears).
 *   3) Load the address into EEAR and the data into EEDR.
 *   4) Select the programming mode in EEPM1:EEPM0.
 *   5) In an interrupts-disabled window: set EEMPE, then within 4 cycles
 *      set EEPE. These two MUST be consecutive or the write is aborted.
 *
 * Reads are simple: set EEAR, set EERE, then read EEDR.
 *
 * Author : (Mohamed Eid)
 */

#include "EEPROM_DRIVER.h"     /* Public types, enums and prototypes */

/*------------------------------------------------------------------------
 * DEFAULT configuration used when EEPROM_init() receives NULL.
 * Safe choice: atomic erase+write mode.
 *----------------------------------------------------------------------*/
static const EEPROM_CONFIG DEFAULT_CONFIG = {
	.m_mode = EEPROM_MODE_ATOMIC      /* Default is the simple, self-erasing atomic mode */
};

/*------------------------------------------------------------------------
 * Cache the selected programming mode so each write can reapply it.
 *----------------------------------------------------------------------*/
static EEPROM_MODE g_mode = EEPROM_MODE_ATOMIC;   /* Mirrors EEPM1:EEPM0; defaults to atomic */

/*------------------------------------------------------------------------
 * Helper: block until the EEPROM is ready to accept a new write.
 *----------------------------------------------------------------------*/
static void eeprom_wait_ready(void)
{
	while(CHECK_BIT(EECR, EEPE));     /* Spin while EEPE is high (a previous EEPROM write is in progress) */
}

/*========================================================================
 * EEPROM_init : remember the programming mode chosen by the caller.
 *======================================================================*/
void EEPROM_init(const EEPROM_CONFIG* config)
{
	if(config == 0)                   /* If the caller passed NULL ... */
	{
		config = &DEFAULT_CONFIG;     /* ... use the safe default configuration */
	}
	g_mode = config->m_mode;          /* Store the mode; applied on every subsequent write */
}

/*========================================================================
 * EEPROM_writeByte : program a single byte (blocking).
 *======================================================================*/
EEPROM_Status EEPROM_writeByte(uint16_t address, uint8_t data)
{
	if(address > EEPROM_LAST_ADDR)    /* Guard against writing outside the 4KB space */
	{
		return EEPROM_ERR_RANGE;      /* Reject and report a range error */
	}

	eeprom_wait_ready();              /* Step 1: wait for any in-flight EEPROM write to finish */
	while(CHECK_BIT(SPMCSR, SPMEN));  /* Step 2: wait for any Flash self-program write to finish */

	EEAR = address;                   /* Step 3a: load the target address into the EEPROM address register */
	EEDR = data;                      /* Step 3b: load the byte to write into the EEPROM data register */

	/* Step 4: program the mode bits EEPM1:EEPM0 in EECR. */
	EECR = (EECR & ~((1 << EEPM1) | (1 << EEPM0)))   /* Clear the old mode bits ... */
	            | (g_mode << EEPM0);                 /* ... write the cached mode (atomic/erase/write) */

	uint8_t sreg = SREG;              /* Save the global interrupt state */
	cli();                            /* Disable interrupts: EEMPE->EEPE must be consecutive (4-cycle rule) */

	SET_BIT(EECR, EEMPE);             /* Step 5a: master write enable (opens the 4-cycle window) */
	SET_BIT(EECR, EEPE);              /* Step 5b: start the actual write (must be the very next write) */

	SREG = sreg;                      /* Restore the previous interrupt state */

	return EEPROM_OK;                 /* The write has started; the cell programs in the background */
}

/*========================================================================
 * EEPROM_readByte : read one byte (blocking until safe).
 *======================================================================*/
EEPROM_Status EEPROM_readByte(uint16_t address, uint8_t* data)
{
	if(address > EEPROM_LAST_ADDR)    /* Bounds check the address */
	{
		return EEPROM_ERR_RANGE;      /* Out of range */
	}
	if(data == 0)                     /* Reject a NULL output pointer */
	{
		return EEPROM_ERR_NULL;       /* Nowhere to store the result */
	}

	eeprom_wait_ready();              /* Make sure no write is still in progress before reading */

	EEAR = address;                   /* Load the address to read from */
	SET_BIT(EECR, EERE);              /* Trigger the read strobe (EERE) */
	*data = EEDR;                      /* Copy the latched byte from EEDR to the caller */

	return EEPROM_OK;                 /* Read complete */
}

/*========================================================================
 * EEPROM_update : write only if the stored value differs (cycle-saving).
 *======================================================================*/
EEPROM_Status EEPROM_update(uint16_t address, uint8_t data)
{
	uint8_t current;                              /* Holds the value already stored */
	EEPROM_Status st = EEPROM_readByte(address, &current);  /* Read what is there now */
	if(st != EEPROM_OK)                           /* If the read failed (range) ... */
	{
		return st;                                /* ... propagate the error */
	}
	if(current == data)                           /* If the cell already holds the desired value ... */
	{
		return EEPROM_OK;                         /* ... skip the write to preserve endurance */
	}
	return EEPROM_writeByte(address, data);       /* Otherwise perform the write */
}

/*========================================================================
 * EEPROM_writeBlock : write 'len' consecutive bytes.
 *======================================================================*/
EEPROM_Status EEPROM_writeBlock(uint16_t address, const uint8_t* data, uint16_t len)
{
	if(data == 0)                                 /* Reject a NULL source pointer */
	{
		return EEPROM_ERR_NULL;
	}
	if((uint32_t)address + len > EEPROM_SIZE_BYTES)   /* Ensure the whole block fits (32-bit math avoids overflow) */
	{
		return EEPROM_ERR_RANGE;
	}

	for(uint16_t i = 0; i < len; i++)             /* Walk byte by byte ... */
	{
		EEPROM_Status st = EEPROM_update(address + i, data[i]);  /* ... using update() to spare unchanged cells */
		if(st != EEPROM_OK)                       /* If any byte failed ... */
		{
			return st;                            /* ... stop and report it */
		}
	}
	return EEPROM_OK;                             /* All bytes written */
}

/*========================================================================
 * EEPROM_readBlock : read 'len' consecutive bytes.
 *======================================================================*/
EEPROM_Status EEPROM_readBlock(uint16_t address, uint8_t* data, uint16_t len)
{
	if(data == 0)                                 /* Reject a NULL destination pointer */
	{
		return EEPROM_ERR_NULL;
	}
	if((uint32_t)address + len > EEPROM_SIZE_BYTES)   /* Bounds check the whole block */
	{
		return EEPROM_ERR_RANGE;
	}

	for(uint16_t i = 0; i < len; i++)             /* Walk byte by byte ... */
	{
		EEPROM_Status st = EEPROM_readByte(address + i, &data[i]);  /* ... reading each cell */
		if(st != EEPROM_OK)                       /* On any failure ... */
		{
			return st;                            /* ... stop and report */
		}
	}
	return EEPROM_OK;                             /* All bytes read */
}
