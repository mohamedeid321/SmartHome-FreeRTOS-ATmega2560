/*
 * RELAY_DRIVER.c
 *
 * Layer  : HAL
 * Target : ATmega2560 @ 16MHz
 * Device : relay module, driven through MCAL/DIO.
 *
 * Author : (Mohamed Eid)
 */

#include "RELAY_DRIVER.h"    /* Public types, enums and prototypes */

/*------------------------------------------------------------------------
 * DEFAULT handle used when RELAY_init() receives a NULL config.
 * PA5, active-low (typical opto-isolated board), starts OFF.
 *----------------------------------------------------------------------*/
const RELAY_Handle RELAY_DEFAULT_CONFIG = {
	.m_port     = DIO_PORTA,          /* Default port A */
	.m_pin      = DIO_PIN5,           /* Default pin 5 */
	.m_polarity = RELAY_ACTIVE_LOW,   /* Default active-low board */
	._state     = RELAY_OFF           /* Logical state starts OFF */
};

/*------------------------------------------------------------------------
 * Helper: drive the control pin to the electrical level that produces the
 * requested logical state, accounting for polarity.
 *----------------------------------------------------------------------*/
static void relay_apply(RELAY_Handle* relay, RELAY_State state)
{
	uint8_t energise = (state == RELAY_ON);              /* Do we want the coil energised? */

	/* Active-low boards energise on LOW, so the pin level is inverted vs. active-high. */
	if(relay->m_polarity == RELAY_ACTIVE_LOW)
	{
		DIO_writePin(relay->m_port, relay->m_pin, energise ? DIO_LOW : DIO_HIGH);   /* ON=LOW, OFF=HIGH */
	}
	else
	{
		DIO_writePin(relay->m_port, relay->m_pin, energise ? DIO_HIGH : DIO_LOW);   /* ON=HIGH, OFF=LOW */
	}

	relay->_state = state;                               /* Cache the new logical state */
}

/*========================================================================
 * RELAY_init : output mode, and force a known-OFF start (safety).
 *======================================================================*/
void RELAY_init(RELAY_Handle* relay, const RELAY_Handle* config)
{
	if(config == 0)              /* NULL-safe: use the default handle */
	{
		config = &RELAY_DEFAULT_CONFIG;
	}
	*relay = *config;            /* Copy the configuration into the caller's handle */

	DIO_setDirection(relay->m_port, relay->m_pin, DIO_OUTPUT);   /* Control pin = output */
	relay_apply(relay, RELAY_OFF);   /* Force OFF immediately so no device wakes up unexpectedly */
}

/*========================================================================
 * RELAY_on : energise (load ON).
 *======================================================================*/
void RELAY_on(RELAY_Handle* relay)
{
	relay_apply(relay, RELAY_ON);    /* Switch the load on */
}

/*========================================================================
 * RELAY_off : de-energise (load OFF).
 *======================================================================*/
void RELAY_off(RELAY_Handle* relay)
{
	relay_apply(relay, RELAY_OFF);   /* Switch the load off */
}

/*========================================================================
 * RELAY_toggle : flip the current state.
 *======================================================================*/
void RELAY_toggle(RELAY_Handle* relay)
{
	if(relay->_state == RELAY_ON)    /* If currently on ... */
	{
		relay_apply(relay, RELAY_OFF);   /* ... turn it off */
	}
	else                             /* If currently off ... */
	{
		relay_apply(relay, RELAY_ON);    /* ... turn it on */
	}
}

/*========================================================================
 * RELAY_getState : report the cached logical state.
 *======================================================================*/
RELAY_State RELAY_getState(const RELAY_Handle* relay)
{
	return relay->_state;            /* ON or OFF, regardless of wiring polarity */
}
