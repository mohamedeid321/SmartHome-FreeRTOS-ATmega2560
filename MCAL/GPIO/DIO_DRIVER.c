/*
 * DIO_DRIVER.c
 *
 * Layer  : MCAL
 * Target : ATmega2560 @ 16MHz
 * Purpose: Implementation of the generic Digital I/O driver declared in DIO_DRIVER.h.
 *
 * Design : ATmega2560 ports are not contiguous in memory, so we cannot compute
 *          a register address by arithmetic alone. Instead we keep three small
 *          lookup tables (DDR / PORT / PIN) indexed by the DIO_PORT enum. This
 *          keeps the code branch-free and easy to extend.
 *
 * Author : (Mohamed Eid)
 */

#include "DIO_DRIVER.h"        /* Pull in the public types, enums and prototypes for this driver */

/*------------------------------------------------------------------------
 * Lookup tables: map a DIO_PORT enum value to the address of its register.
 * 'volatile uint8_t*' because hardware registers can change outside program flow.
 * The order MUST match the DIO_PORT enum order (A,B,C,D,E,F,G,H,J,K,L).
 *----------------------------------------------------------------------*/
static volatile uint8_t* const DDR_REG[]  = { &DDRA,&DDRB,&DDRC,&DDRD,&DDRE,&DDRF,&DDRG,&DDRH,&DDRJ,&DDRK,&DDRL };   /* Direction registers */
static volatile uint8_t* const PORT_REG[] = { &PORTA,&PORTB,&PORTC,&PORTD,&PORTE,&PORTF,&PORTG,&PORTH,&PORTJ,&PORTK,&PORTL }; /* Output / pull-up registers */
static volatile uint8_t* const PIN_REG[]  = { &PINA,&PINB,&PINC,&PIND,&PINE,&PINF,&PING,&PINH,&PINJ,&PINK,&PINL };   /* Input (read) registers */

/*------------------------------------------------------------------------
 * DEFAULT configuration used when DIO_init() is called with NULL.
 * Safe choice: Port A, Pin 0, as a LOW output. 'static const' so it lives
 * in flash and is private to this file.
 *----------------------------------------------------------------------*/
static const DIO_CONFIG DEFAULT_CONFIG = {
	.m_port       = DIO_PORTA,    /* Default port */
	.m_pin        = DIO_PIN0,     /* Default pin */
	.m_direction  = DIO_OUTPUT,   /* Default direction is output */
	.m_init_level = DIO_LOW       /* Default output starts LOW */
};

/*========================================================================
 * DIO_init : configure a single pin from a DIO_CONFIG block.
 *======================================================================*/
void DIO_init(const DIO_CONFIG* config)
{
	if(config == 0)                       /* If the caller passed NULL ... */
	{
		config = &DEFAULT_CONFIG;         /* ... fall back to the safe default configuration */
	}

	/* Apply the requested direction (this also handles the pull-up case). */
	DIO_setDirection(config->m_port, config->m_pin, config->m_direction);

	/* If the pin is an OUTPUT, drive it to the requested initial level. */
	if(config->m_direction == DIO_OUTPUT)
	{
		DIO_writePin(config->m_port, config->m_pin, config->m_init_level);
	}
}

/*========================================================================
 * DIO_setDirection : set a pin to input, input-with-pullup, or output.
 *======================================================================*/
void DIO_setDirection(DIO_PORT port, DIO_PIN pin, DIO_DIRECTION dir)
{
	switch(dir)                                          /* Decide based on the requested direction */
	{
		case DIO_OUTPUT:                                 /* --- configure as push-pull output --- */
			SET_BIT(*DDR_REG[port], pin);                /* DDR bit = 1 makes the pin an output */
			break;

		case DIO_INPUT:                                  /* --- configure as floating input --- */
			CLEAR_BIT(*DDR_REG[port], pin);              /* DDR bit = 0 makes the pin an input */
			CLEAR_BIT(*PORT_REG[port], pin);             /* PORT bit = 0 disables the internal pull-up */
			break;

		case DIO_INPUT_PULLUP:                           /* --- configure as input with pull-up --- */
			CLEAR_BIT(*DDR_REG[port], pin);              /* DDR bit = 0 makes the pin an input */
			SET_BIT(*PORT_REG[port], pin);               /* PORT bit = 1 enables the internal pull-up */
			break;
	}
}

/*========================================================================
 * DIO_writePin : drive an output pin HIGH or LOW.
 *======================================================================*/
void DIO_writePin(DIO_PORT port, DIO_PIN pin, DIO_LEVEL level)
{
	if(level == DIO_HIGH)                    /* If the caller wants a logic 1 ... */
	{
		SET_BIT(*PORT_REG[port], pin);       /* ... set the matching PORT bit HIGH */
	}
	else                                     /* Otherwise the caller wants a logic 0 ... */
	{
		CLEAR_BIT(*PORT_REG[port], pin);     /* ... clear the matching PORT bit LOW */
	}
}

/*========================================================================
 * DIO_togglePin : invert the current state of an output pin.
 *======================================================================*/
void DIO_togglePin(DIO_PORT port, DIO_PIN pin)
{
	TOGGLE_BIT(*PORT_REG[port], pin);        /* XOR the PORT bit with 1 to flip it */
}

/*========================================================================
 * DIO_readPin : read the live level on any pin (input or output).
 *======================================================================*/
DIO_LEVEL DIO_readPin(DIO_PORT port, DIO_PIN pin)
{
	if(CHECK_BIT(*PIN_REG[port], pin))       /* Read from the PIN register (the real pin state) */
	{
		return DIO_HIGH;                     /* Bit was set -> pin is HIGH */
	}
	return DIO_LOW;                          /* Bit was clear -> pin is LOW */
}

/*========================================================================
 * DIO_writePort : write all 8 bits of a port in one operation.
 *======================================================================*/
void DIO_writePort(DIO_PORT port, uint8_t value)
{
	*PORT_REG[port] = value;                 /* Overwrite the whole output register with 'value' */
}

/*========================================================================
 * DIO_readPort : read all 8 bits of a port in one operation.
 *======================================================================*/
uint8_t DIO_readPort(DIO_PORT port)
{
	return *PIN_REG[port];                   /* Return the raw 8-bit input register value */
}
