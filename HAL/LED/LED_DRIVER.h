/*
 * LED_DRIVER.h
 *
 * Layer  : HAL (Hardware Abstraction Layer)
 * Target : ATmega2560 @ 16MHz
 * Device : single LED on one GPIO pin.
 * Depends: MCAL/DIO.
 *
 * Design note - "handle" style:
 *   A system can have many LEDs. Rather than one global LED, each LED is an
 *   LED_Handle the caller owns. You init a handle once, then pass it to
 *   on/off/toggle. This lets the same driver code drive any number of LEDs.
 *
 * Polarity:
 *   LED_ACTIVE_HIGH -> pin HIGH lights the LED (pin -> LED -> R -> GND).
 *   LED_ACTIVE_LOW  -> pin LOW  lights the LED (VCC -> LED -> R -> pin).
 *   LED_on() always means "light it", whichever wiring you chose.
 *
 * Author : (Mohamed Eid)
 */

#ifndef LED_DRIVER_H_          /* Include guard opening */
#define LED_DRIVER_H_          /* Guard token */

#include <stdint.h>            /* Fixed-width integer types */
#include "DIO_DRIVER.h"        /* Sits on top of the MCAL DIO driver */

/*------------------------------------------------------------------------
 * ENUM : electrical polarity of the LED wiring.
 *----------------------------------------------------------------------*/
typedef enum {
	LED_ACTIVE_HIGH = 0,   /* Pin HIGH = lit */
	LED_ACTIVE_LOW         /* Pin LOW  = lit */
}LED_Polarity;

/*------------------------------------------------------------------------
 * STRUCT : an LED instance ("handle"). Fill the first three fields, then
 * call LED_init() on it; the driver uses it for all later calls.
 *----------------------------------------------------------------------*/
typedef struct {
	DIO_PORT     m_port;       /* Port the LED pin is on */
	DIO_PIN      m_pin;        /* Pin index */
	LED_Polarity m_polarity;   /* Active-high or active-low */
}LED_Handle;

/*================================ APIs ==================================*/

/* Configure the LED pin as an output and start it OFF. Pass NULL to use
 * LED_DEFAULT_CONFIG (which is copied into *led for you). */
void LED_init(LED_Handle* led, const LED_Handle* config);

/* Turn the LED on (light it), honouring its polarity. */
void LED_on(const LED_Handle* led);

/* Turn the LED off, honouring its polarity. */
void LED_off(const LED_Handle* led);

/* Toggle the LED (lit<->dark). */
void LED_toggle(const LED_Handle* led);

/* Ready-made default handle (PB0, active-high). */
extern const LED_Handle LED_DEFAULT_CONFIG;

#endif /* LED_DRIVER_H_ */
