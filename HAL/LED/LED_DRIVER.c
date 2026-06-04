/*
 * LED_DRIVER.c
 *
 * Layer  : HAL
 * Target : ATmega2560 @ 16MHz
 * Device : single LED on a GPIO pin, driven through MCAL/DIO.
 *
 * Author : (Mohamed Eid)
 */

#include "LED_DRIVER.h"      /* Public types, enums and prototypes */

/*------------------------------------------------------------------------
 * DEFAULT handle used when LED_init() receives a NULL config.
 * PB0, active-high (the common LED-to-GND wiring).
 *----------------------------------------------------------------------*/
const LED_Handle LED_DEFAULT_CONFIG = {
	.m_port     = DIO_PORTB,         /* Default port B */
	.m_pin      = DIO_PIN0,          /* Default pin 0 */
	.m_polarity = LED_ACTIVE_HIGH    /* Default active-high */
};

/*========================================================================
 * LED_init : set the pin as output and leave the LED OFF.
 *======================================================================*/
void LED_init(LED_Handle* led, const LED_Handle* config)
{
	if(config == 0)              /* NULL-safe: use the default handle ... */
	{
		config = &LED_DEFAULT_CONFIG;
	}
	*led = *config;              /* Copy the configuration into the caller's handle */

	DIO_setDirection(led->m_port, led->m_pin, DIO_OUTPUT);   /* The LED pin is an output */
	LED_off(led);                /* Start in the OFF state (polarity handled in LED_off) */
}

/*========================================================================
 * LED_on : light the LED, accounting for polarity.
 *======================================================================*/
void LED_on(const LED_Handle* led)
{
	if(led->m_polarity == LED_ACTIVE_HIGH)                  /* Active-high wiring ... */
	{
		DIO_writePin(led->m_port, led->m_pin, DIO_HIGH);    /* ... HIGH lights it */
	}
	else                                                    /* Active-low wiring ... */
	{
		DIO_writePin(led->m_port, led->m_pin, DIO_LOW);     /* ... LOW lights it */
	}
}

/*========================================================================
 * LED_off : turn the LED off, accounting for polarity (the inverse of on).
 *======================================================================*/
void LED_off(const LED_Handle* led)
{
	if(led->m_polarity == LED_ACTIVE_HIGH)                  /* Active-high wiring ... */
	{
		DIO_writePin(led->m_port, led->m_pin, DIO_LOW);     /* ... LOW turns it off */
	}
	else                                                    /* Active-low wiring ... */
	{
		DIO_writePin(led->m_port, led->m_pin, DIO_HIGH);    /* ... HIGH turns it off */
	}
}

/*========================================================================
 * LED_toggle : flip the pin. Works for either polarity because it just
 * inverts whatever the current electrical state is.
 *======================================================================*/
void LED_toggle(const LED_Handle* led)
{
	DIO_togglePin(led->m_port, led->m_pin);   /* Invert the pin -> lit<->dark */
}
