/*
 * BUZZER_DRIVER.h
 *
 * Layer  : HAL (Hardware Abstraction Layer)
 * Target : ATmega2560 @ 16MHz
 * Device : buzzer (active or passive).
 * Depends: MCAL/DIO  (active mode) and MCAL/TIMER (passive PWM mode).
 *
 * Two buzzer types, one driver:
 *   ACTIVE  : has a built-in oscillator. Drive the pin HIGH and it sounds;
 *             LOW and it stops. We use DIO only - simple and safe.
 *   PASSIVE : no oscillator; it needs a square wave. We use a timer in
 *             Fast-PWM so the timer toggles its OC pin to make the tone.
 *             The buzzer MUST be wired to that timer's OC pin:
 *               Timer0 -> OC0A = PB7
 *               Timer2 -> OC2A = PB4
 *
 * Author : (Mohamed Eid)
 */

#ifndef BUZZER_DRIVER_H_       /* Include guard opening */
#define BUZZER_DRIVER_H_       /* Guard token */

#include <stdint.h>            /* Fixed-width integer types */
#include "DIO_DRIVER.h"        /* Active mode uses DIO */
#include "TIMER_DRIVER.h"      /* Passive mode uses a timer's PWM */

/*------------------------------------------------------------------------
 * ENUM : buzzer type.
 *----------------------------------------------------------------------*/
typedef enum {
	BUZZER_ACTIVE = 0,   /* Built-in oscillator: on/off via DIO */
	BUZZER_PASSIVE       /* Needs a tone: driven by timer PWM on the OC pin */
}BUZZER_Type;

/*------------------------------------------------------------------------
 * ENUM : drive polarity (some buzzer modules are active-low via a driver
 * transistor). Only used in ACTIVE mode.
 *----------------------------------------------------------------------*/
typedef enum {
	BUZZER_ACTIVE_HIGH = 0,   /* Pin HIGH = sound */
	BUZZER_ACTIVE_LOW         /* Pin LOW  = sound */
}BUZZER_Polarity;

/*------------------------------------------------------------------------
 * STRUCT : buzzer configuration.
 *   ACTIVE  uses m_port/m_pin/m_polarity.
 *   PASSIVE uses m_timer (its OC pin carries the tone; see header notes).
 *----------------------------------------------------------------------*/
typedef struct {
	BUZZER_Type     m_type;        /* Active or passive */
	DIO_PORT        m_port;        /* (active) port of the buzzer pin */
	DIO_PIN         m_pin;         /* (active) pin index */
	BUZZER_Polarity m_polarity;    /* (active) drive polarity */
	TIMER_SELECT_ID m_timer;       /* (passive) which timer drives the OC pin */
}BUZZER_Config;

/*================================ APIs ==================================*/

/* Initialise the buzzer. Pass NULL to use BUZZER_DEFAULT_CONFIG. */
void BUZZER_init(const BUZZER_Config* config);

/* Start sounding. In passive mode this plays a ~2-3 kHz default tone. */
void BUZZER_on(void);

/* Stop sounding. */
void BUZZER_off(void);

/* Passive mode only: set the tone frequency in Hz (e.g. 1000 = 1 kHz).
 * Ignored in active mode (its pitch is fixed by the hardware). */
void BUZZER_setTone(uint16_t freq_hz);

/* Convenience: sound for 'ms' milliseconds, then stop (blocking).
 * Good for short feedback beeps. */
void BUZZER_beep(uint16_t ms);

/* Ready-made default config (active buzzer on PB1, active-high). */
extern const BUZZER_Config BUZZER_DEFAULT_CONFIG;

#endif /* BUZZER_DRIVER_H_ */
