/*
 * EXT_INT_DRIVER.c
 *
 * Layer  : MCAL
 * Target : ATmega2560 @ 16MHz
 * Purpose: Implementation of the external interrupt driver.
 *
 * Register notes for ATmega2560:
 *   - Sense bits for INT0..INT3 live in EICRA (2 bits each: ISC0x..ISC3x).
 *   - Sense bits for INT4..INT7 live in EICRB (2 bits each: ISC4x..ISC7x).
 *   - The per-line enable mask is EIMSK (bit n enables INTn).
 *   - The pending flag register is EIFR (write 1 to clear a stale flag).
 *   - INT0..3 share PORTD pins PD0..PD3; INT4..7 share PORTE pins PE4..PE7.
 *
 * Author : (Mohamed Eid)
 */

#include "EXT_INT_DRIVER.h"    /* Public types, enums and prototypes for this driver */

/*------------------------------------------------------------------------
 * Callback table: one slot per INT line. Filled by EXT_INT_init /
 * EXT_INT_setCallback and read by the ISRs. 'volatile' because the ISRs
 * (asynchronous to main flow) read these pointers.
 *----------------------------------------------------------------------*/
static void (*volatile extint_callback[8])(void) = {0,0,0,0,0,0,0,0};   /* All slots start empty (NULL) */

/*------------------------------------------------------------------------
 * DEFAULT configuration used when EXT_INT_init() receives NULL.
 * Safe choice: INT0 (PD0), falling edge, pull-up enabled, no callback.
 *----------------------------------------------------------------------*/
static const EXT_INT_CONFIG DEFAULT_CONFIG = {
	.m_line     = EXT_INT0,               /* Default line is INT0 */
	.m_sense    = EXT_INT_FALLING_EDGE,   /* Default trigger is a falling edge (button press to GND) */
	.m_pull     = EXT_INT_PULLUP,         /* Default keeps the pin idle-HIGH via internal pull-up */
	.callback   = 0                       /* Default has no callback attached */
};

/*------------------------------------------------------------------------
 * Helper: make the physical pin behind an INT line an input, and set or
 * clear its internal pull-up. Keeps the public init function readable.
 *----------------------------------------------------------------------*/
static void extint_setup_pin(EXT_INT_LINE line, EXT_INT_PULL pull)
{
	if(line <= EXT_INT3)                          /* INT0..INT3 are on PORTD pins PD0..PD3 */
	{
		CLEAR_BIT(DDRD, line);                    /* DDR bit = 0 -> the pin is an input (line index == pin index here) */
		if(pull == EXT_INT_PULLUP)                /* If the user asked for a pull-up ... */
			SET_BIT(PORTD, line);                 /* ... PORT bit = 1 enables the internal pull-up */
		else                                      /* Otherwise ... */
			CLEAR_BIT(PORTD, line);               /* ... PORT bit = 0 leaves the pin floating */
	}
	else                                          /* INT4..INT7 are on PORTE pins PE4..PE7 */
	{
		uint8_t pin = line;                       /* For INT4..7 the line index already equals the PE pin index (4..7) */
		CLEAR_BIT(DDRE, pin);                     /* DDR bit = 0 -> input */
		if(pull == EXT_INT_PULLUP)                /* Pull-up requested? */
			SET_BIT(PORTE, pin);                  /* Enable internal pull-up */
		else
			CLEAR_BIT(PORTE, pin);                /* Leave floating */
	}
}

/*========================================================================
 * EXT_INT_setSense : program the 2 sense bits for one line.
 *======================================================================*/
void EXT_INT_setSense(EXT_INT_LINE line, EXT_INT_SENSE sense)
{
	if(line <= EXT_INT3)                          /* INT0..INT3 use register EICRA */
	{
		uint8_t shift = line * 2;                 /* Each line owns 2 bits; bit position = line*2 */
		EICRA = (EICRA & ~(0x03 << shift))        /* Clear the old 2 sense bits for this line ... */
		             | ((sense & 0x03) << shift);  /* ... then write the new 2-bit sense code in place */
	}
	else                                          /* INT4..INT7 use register EICRB */
	{
		uint8_t shift = (line - 4) * 2;           /* INT4 starts at bit 0 of EICRB, INT5 at bit 2, etc. */
		EICRB = (EICRB & ~(0x03 << shift))        /* Clear the old 2 sense bits ... */
		             | ((sense & 0x03) << shift);  /* ... write the new sense code */
	}
}

/*========================================================================
 * EXT_INT_setCallback : attach/replace the handler for one line.
 *======================================================================*/
void EXT_INT_setCallback(EXT_INT_LINE line, void (*callback)(void))
{
	extint_callback[line] = callback;             /* Store the function pointer in this line's slot */
}

/*========================================================================
 * EXT_INT_enable : unmask one line so its interrupt can fire.
 *======================================================================*/
void EXT_INT_enable(EXT_INT_LINE line)
{
	SET_BIT(EIFR,  line);                         /* Write 1 to the pending flag to discard any stale event */
	SET_BIT(EIMSK, line);                         /* Set the mask bit -> this INT line is now armed */
}

/*========================================================================
 * EXT_INT_disable : mask one line so it stops firing.
 *======================================================================*/
void EXT_INT_disable(EXT_INT_LINE line)
{
	CLEAR_BIT(EIMSK, line);                       /* Clear the mask bit -> the line is ignored */
}

/*========================================================================
 * EXT_INT_init : configure one external interrupt line end to end.
 *======================================================================*/
void EXT_INT_init(const EXT_INT_CONFIG* config)
{
	if(config == 0)                               /* If the caller passed NULL ... */
	{
		config = &DEFAULT_CONFIG;                 /* ... use the safe default configuration */
	}

	extint_setup_pin(config->m_line, config->m_pull);          /* 1) Make the pin an input + set its pull-up */
	EXT_INT_setSense(config->m_line, config->m_sense);         /* 2) Program the edge/level that triggers it */
	EXT_INT_setCallback(config->m_line, config->callback);     /* 3) Remember the user's handler */
	EXT_INT_enable(config->m_line);                            /* 4) Arm the line (clears stale flag + unmasks) */

	sei();                                        /* Set the global interrupt enable so ISRs can run */
}

/*========================================================================
 * Interrupt Service Routines : one per line. Each simply forwards to the
 * registered callback (if any). Keep ISR bodies tiny.
 *======================================================================*/
ISR(INT0_vect){ if(extint_callback[0]) extint_callback[0](); }   /* INT0 fired -> call its handler */
ISR(INT1_vect){ if(extint_callback[1]) extint_callback[1](); }   /* INT1 fired */
ISR(INT2_vect){ if(extint_callback[2]) extint_callback[2](); }   /* INT2 fired */
ISR(INT3_vect){ if(extint_callback[3]) extint_callback[3](); }   /* INT3 fired */
ISR(INT4_vect){ if(extint_callback[4]) extint_callback[4](); }   /* INT4 fired */
ISR(INT5_vect){ if(extint_callback[5]) extint_callback[5](); }   /* INT5 fired */
ISR(INT6_vect){ if(extint_callback[6]) extint_callback[6](); }   /* INT6 fired */
ISR(INT7_vect){ if(extint_callback[7]) extint_callback[7](); }   /* INT7 fired */
