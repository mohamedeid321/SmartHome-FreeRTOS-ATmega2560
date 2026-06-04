/*
 * App_Fault.h
 *
 * Layer  : APP (pure logic)
 * Target : ATmega2560
 *
 * Fault detection. Rather than only checking "is it above a threshold", this
 * module flags readings that are physically IMPOSSIBLE - which usually means
 * a sensor is disconnected, shorted, or miswired. That is real fault
 * detection: distinguishing a hot room (high but valid) from a broken sensor
 * (out of range). Results feed the Faults screen and the log.
 *
 * Author : (Mohamed Eid)
 */

#ifndef APP_FAULT_H_          /* Include guard opening */
#define APP_FAULT_H_          /* Guard token */

#include <stdint.h>           /* Fixed-width integer types */
#include "App_Types.h"        /* FaultCode, SystemState */

/* Plausible physical limits. A real room temperature can't be these. */
#define FAULT_TEMP_MIN_C      0     /* Below this: sensor likely shorted to GND */
#define FAULT_TEMP_MAX_C      80    /* Above this: sensor likely disconnected/open */

/*================================ APIs ==================================*/

/* Check a temperature reading. Returns a FaultCode:
 *   FAULT_NONE        : reading is plausible
 *   FAULT_TEMP_SENSOR : reading is physically impossible (wiring fault)
 *   FAULT_OVER_TEMP   : reading is valid but over the alarm threshold */
FaultCode App_Fault_checkTemp(uint16_t temperature_c);

/* Check a light percentage (0..100). Anything outside that range means the
 * conversion or wiring is wrong. Returns FAULT_NONE or FAULT_LIGHT_SENSOR. */
FaultCode App_Fault_checkLight(uint8_t light_percent);

/* Convert a fault code to a short human string for the LCD / terminal. */
const char* App_Fault_toString(FaultCode code);

#endif /* APP_FAULT_H_ */
