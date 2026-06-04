/*
 * RELAY_DRIVER.h
 *
 * Layer  : HAL (Hardware Abstraction Layer)
 * Target : ATmega2560 @ 16MHz
 * Device : relay module (electromagnetic switch for mains/high-power loads).
 * Depends: MCAL/DIO.
 *
 * Safety & polarity:
 *   A relay switches REAL devices (lamps, fans), so it must power up in a
 *   known OFF state - RELAY_init() guarantees that.
 *   Most ready-made relay boards (with an opto-isolated transistor stage)
 *   are ACTIVE-LOW: a LOW on the control pin energises the coil. This
 *   driver defaults to active-low and exposes the polarity in the handle so
 *   RELAY_on() always means "energise / close the load circuit".
 *
 * Author : (Mohamed Eid)
 */

#ifndef RELAY_DRIVER_H_        /* Include guard opening */
#define RELAY_DRIVER_H_        /* Guard token */

#include <stdint.h>            /* Fixed-width integer types */
#include "DIO_DRIVER.h"        /* Sits on top of the MCAL DIO driver */

/*------------------------------------------------------------------------
 * ENUM : control polarity of the relay board.
 *----------------------------------------------------------------------*/
typedef enum {
	RELAY_ACTIVE_LOW = 0,   /* LOW energises the relay (most opto-isolated boards) */
	RELAY_ACTIVE_HIGH       /* HIGH energises the relay (bare coil + transistor) */
}RELAY_Polarity;

/*------------------------------------------------------------------------
 * ENUM : logical relay state.
 *----------------------------------------------------------------------*/
typedef enum {
	RELAY_OFF = 0,   /* Load circuit open (device off) */
	RELAY_ON         /* Load circuit closed (device on) */
}RELAY_State;

/*------------------------------------------------------------------------
 * STRUCT : a relay instance ("handle").
 *----------------------------------------------------------------------*/
typedef struct {
	DIO_PORT       m_port;       /* Port of the relay control pin */
	DIO_PIN        m_pin;        /* Pin index */
	RELAY_Polarity m_polarity;   /* Active-low or active-high */
	RELAY_State    _state;       /* (internal) cached current state */
}RELAY_Handle;

/*================================ APIs ==================================*/

/* Configure the pin as output and force the relay OFF. Pass NULL config to
 * copy RELAY_DEFAULT_CONFIG into *relay. */
void RELAY_init(RELAY_Handle* relay, const RELAY_Handle* config);

/* Energise the relay (turn the load ON), honouring polarity. */
void RELAY_on(RELAY_Handle* relay);

/* De-energise the relay (turn the load OFF), honouring polarity. */
void RELAY_off(RELAY_Handle* relay);

/* Flip the relay between ON and OFF. */
void RELAY_toggle(RELAY_Handle* relay);

/* Return the current logical state (RELAY_ON / RELAY_OFF). */
RELAY_State RELAY_getState(const RELAY_Handle* relay);

/* Ready-made default handle (PA5, active-low, starts OFF). */
extern const RELAY_Handle RELAY_DEFAULT_CONFIG;

#endif /* RELAY_DRIVER_H_ */
