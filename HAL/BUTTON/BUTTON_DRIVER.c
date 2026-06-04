/*
 * BUTTON_DRIVER.c
 *
 * Layer  : HAL
 * Target : ATmega2560 @ 16MHz
 * Device : push button on a GPIO pin, debounced through periodic sampling.
 *
 * Debounce model (non-blocking):
 *   Each BUTTON_update() reads the raw pin once. A change is only accepted
 *   after BUTTON_DEBOUNCE_SAMPLES consecutive identical raw readings. Since
 *   update() is called every ~10-20 ms from a task, 3 samples means a state
 *   must hold steady for ~30-60 ms, which comfortably outlasts contact
 *   bounce - without ever blocking the CPU.
 *
 * Author : (Mohamed Eid)
 */

#include "BUTTON_DRIVER.h"   /* Public types, enums and prototypes */

/*------------------------------------------------------------------------
 * DEFAULT handle used when BUTTON_init() receives a NULL config.
 * PD0, active-low (button to GND with the internal pull-up).
 *----------------------------------------------------------------------*/
const BUTTON_Handle BUTTON_DEFAULT_CONFIG = {
	.m_port        = DIO_PORTD,           /* Default port D */
	.m_pin         = DIO_PIN0,            /* Default pin 0 */
	.m_polarity    = BUTTON_ACTIVE_LOW,   /* Default active-low */
	._stable_state = 0,                   /* Start "not pressed" */
	._candidate    = 0,                   /* No pending candidate yet */
	._count        = 0,                   /* No samples counted yet */
	._edge_consumed= 0                    /* No edge waiting */
};

/*------------------------------------------------------------------------
 * Helper: read the RAW pressed state (1 = pressed) accounting for polarity.
 *----------------------------------------------------------------------*/
static uint8_t button_raw_pressed(const BUTTON_Handle* btn)
{
	DIO_LEVEL level = DIO_readPin(btn->m_port, btn->m_pin);   /* Read the actual pin level */

	if(btn->m_polarity == BUTTON_ACTIVE_LOW)                 /* Active-low: pressed = LOW */
	{
		return (level == DIO_LOW) ? 1 : 0;                   /* LOW means pressed */
	}
	return (level == DIO_HIGH) ? 1 : 0;                      /* Active-high: HIGH means pressed */
}

/*========================================================================
 * BUTTON_init : configure the pin and clear the debounce state.
 *======================================================================*/
void BUTTON_init(BUTTON_Handle* btn, const BUTTON_Handle* config)
{
	if(config == 0)              /* NULL-safe: use the default handle */
	{
		config = &BUTTON_DEFAULT_CONFIG;
	}
	*btn = *config;              /* Copy the configuration into the caller's handle */

	/* Active-low buttons need the internal pull-up; active-high expect an
	 * external pull-down, so a plain input is correct there. */
	if(btn->m_polarity == BUTTON_ACTIVE_LOW)
	{
		DIO_setDirection(btn->m_port, btn->m_pin, DIO_INPUT_PULLUP);   /* Input + pull-up */
	}
	else
	{
		DIO_setDirection(btn->m_port, btn->m_pin, DIO_INPUT);          /* Plain input */
	}

	/* Reset the internal debounce/edge state to a clean "released". */
	btn->_stable_state  = 0;     /* Debounced state = released */
	btn->_candidate     = 0;     /* No candidate change pending */
	btn->_count         = 0;     /* No matching samples yet */
	btn->_edge_consumed = 0;     /* No fresh edge pending */
}

/*========================================================================
 * BUTTON_update : one debounce step. Call periodically from a task.
 *======================================================================*/
void BUTTON_update(BUTTON_Handle* btn)
{
	uint8_t raw = button_raw_pressed(btn);   /* Take one raw sample (1=pressed,0=released) */

	if(raw == btn->_stable_state)            /* Raw already matches the accepted state? */
	{
		btn->_count = 0;                     /* Nothing changing: reset the candidate counter */
		return;                              /* Done for this sample */
	}

	/* Raw differs from the stable state -> a possible change is in progress. */
	if(raw == btn->_candidate)               /* Same candidate as last time? */
	{
		btn->_count++;                       /* One more matching sample */
	}
	else                                     /* A different candidate value appeared */
	{
		btn->_candidate = raw;               /* Track this new candidate ... */
		btn->_count     = 1;                 /* ... starting its count at 1 */
	}

	if(btn->_count >= BUTTON_DEBOUNCE_SAMPLES)   /* Held steady long enough? */
	{
		btn->_stable_state = raw;            /* Accept the new debounced state */
		btn->_count        = 0;              /* Reset the counter for the next change */

		if(raw == 1)                         /* A confirmed press (release->press edge) */
		{
			btn->_edge_consumed = 0;         /* Arm a fresh edge for wasPressed() */
		}
	}
}

/*========================================================================
 * BUTTON_isPressed : LEVEL query - is it held down right now (debounced)?
 *======================================================================*/
uint8_t BUTTON_isPressed(const BUTTON_Handle* btn)
{
	return btn->_stable_state;               /* 1 while held, 0 while released */
}

/*========================================================================
 * BUTTON_wasPressed : EDGE query - true once per new press.
 *======================================================================*/
uint8_t BUTTON_wasPressed(BUTTON_Handle* btn)
{
	if(btn->_stable_state == 1 && btn->_edge_consumed == 0)   /* Pressed AND edge not yet read */
	{
		btn->_edge_consumed = 1;             /* Consume it so the same press is not reported twice */
		return 1;                            /* Report the new press exactly once */
	}
	return 0;                                /* Otherwise nothing new */
}
