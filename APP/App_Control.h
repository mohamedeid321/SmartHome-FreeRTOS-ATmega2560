/*
 * App_Control.h
 *
 * Layer  : APP (pure business logic - no hardware, no RTOS)
 * Target : ATmega2560
 *
 * The AUTO-mode decision rules. These functions take sensor readings and
 * return DECISIONS (what the fan/light SHOULD do). They do NOT drive any
 * pin - that is the task layer's job. Keeping them pure means they can be
 * unit-tested on a PC with no microcontroller at all.
 *
 * Author : (Mohamed Eid)
 */

#ifndef APP_CONTROL_H_        /* Include guard opening */
#define APP_CONTROL_H_        /* Guard token */

#include <stdint.h>           /* Fixed-width integer types */
#include "App_Types.h"        /* FanState and other shared types */

/*================================ APIs ==================================*/

/* Decide the fan state from a temperature in whole degrees C.
 * Rules (from SystemConfig): <25 OFF, 25..35 MED, >35 HIGH. */
FanState App_Control_decideFan(uint16_t temperature_c);

/* Decide whether the lamp should be ON from a light percentage 0..100.
 * Rule: ON when darker than CFG_LIGHT_ON_PERCENT. Returns 1 = on, 0 = off. */
uint8_t App_Control_decideLight(uint8_t light_percent);

/* Decide whether the over-temperature alarm should sound.
 * Returns 1 when temperature_c >= CFG_TEMP_ALARM. */
uint8_t App_Control_isOverTemp(uint16_t temperature_c);

/* Map a FanState to a PWM speed percentage (0 / 50 / 100). */
uint8_t App_Control_fanSpeedPercent(FanState fan);

#endif /* APP_CONTROL_H_ */
