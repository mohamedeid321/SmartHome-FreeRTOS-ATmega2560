/*
 * BUTTON_DRIVER.h
 *
 * Layer  : HAL (Hardware Abstraction Layer)
 * Target : ATmega2560 @ 16MHz
 * Device : momentary push button on one GPIO pin.
 * Depends: MCAL/DIO.
 *
 * Design:
 *   - Handle style (like LED): each button is its own BUTTON_Handle, so the
 *     system can have many buttons sharing one driver.
 *   - Polarity aware: active-low (button to GND + pull-up, the common case)
 *     or active-high.
 *   - Two ways to ask about state:
 *       BUTTON_isPressed() -> LEVEL  : "is it held down right now?"
 *       BUTTON_wasPressed() -> EDGE  : "did a NEW press happen since last call?"
 *     wasPressed() is what you usually want (one event per physical press).
 *   - Software debounce: a short confirmation window filters contact bounce.
 *     This is a non-blocking, sampling debounce meant to be called
 *     periodically from a FreeRTOS task (e.g. every 10-20 ms).
 *
 * Author : (Mohamed Eid)
 */

#ifndef BUTTON_DRIVER_H_       /* Include guard opening */
#define BUTTON_DRIVER_H_       /* Guard token */

#include <stdint.h>            /* Fixed-width integer types */
#include "DIO_DRIVER.h"        /* Sits on top of the MCAL DIO driver */

/*------------------------------------------------------------------------
 * ENUM : electrical polarity of the button wiring.
 *----------------------------------------------------------------------*/
typedef enum {
	BUTTON_ACTIVE_LOW = 0,   /* Pressed = pin LOW (button to GND + pull-up) - common */
	BUTTON_ACTIVE_HIGH       /* Pressed = pin HIGH (button to VCC + pull-down) */
}BUTTON_Polarity;

/*------------------------------------------------------------------------
 * STRUCT : a button instance ("handle"). The last two fields are internal
 * debounce state the driver maintains for you - do not touch them.
 *----------------------------------------------------------------------*/
typedef struct {
	DIO_PORT        m_port;        /* Port the button pin is on */
	DIO_PIN         m_pin;         /* Pin index */
	BUTTON_Polarity m_polarity;    /* Active-low or active-high */

	uint8_t  _stable_state;        /* (internal) last debounced state: 1 = pressed */
	uint8_t  _candidate;           /* (internal) raw reading awaiting confirmation */
	uint8_t  _count;               /* (internal) how many matching samples seen so far */
	uint8_t  _edge_consumed;       /* (internal) guards wasPressed() against repeats */
}BUTTON_Handle;

/* How many consecutive equal samples confirm a new state (the debounce depth). */
#define BUTTON_DEBOUNCE_SAMPLES   3

/*================================ APIs ==================================*/

/* Configure the button pin (input + pull-up for active-low). Pass NULL config
 * to copy BUTTON_DEFAULT_CONFIG into *btn. */
void BUTTON_init(BUTTON_Handle* btn, const BUTTON_Handle* config);

/* Sample + debounce. Call this periodically (e.g. every 10-20 ms from a task).
 * It updates the internal stable state used by the query functions below. */
void BUTTON_update(BUTTON_Handle* btn);

/* LEVEL: returns 1 while the button is held down (debounced). */
uint8_t BUTTON_isPressed(const BUTTON_Handle* btn);

/* EDGE: returns 1 exactly once per new press, then 0 until released+pressed again. */
uint8_t BUTTON_wasPressed(BUTTON_Handle* btn);

/* Ready-made default handle (PD0, active-low). */
extern const BUTTON_Handle BUTTON_DEFAULT_CONFIG;

#endif /* BUTTON_DRIVER_H_ */
