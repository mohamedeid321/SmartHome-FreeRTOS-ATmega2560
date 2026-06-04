/*
 * App_SM.h
 *
 * Layer  : APP (the central state machine - pure logic)
 * Target : ATmega2560
 *
 * This is the brain. It owns NO hardware and calls NO RTOS function. It takes
 * the current SystemState plus an incoming AppEvent and updates the state
 * accordingly: switching modes, running the AUTO control rules, driving the
 * SECURITY sub-machine, and recording faults. The task layer is responsible
 * for actually reading inputs, feeding events in, and applying the resulting
 * SystemState to the physical actuators.
 *
 * Why this split: the entire decision logic of the product lives in one
 * testable place, independent of FreeRTOS and the AVR.
 *
 * Author : (Mohamed Eid)
 */

#ifndef APP_SM_H_             /* Include guard opening */
#define APP_SM_H_             /* Guard token */

#include "App_Types.h"        /* SystemState, AppEvent */
#include "App_Security.h"     /* SecurityHooks */

/*================================ APIs ==================================*/

/* Initialise the state machine and the modules it owns (security).
 * 'startup_mode' is the mode to begin in (e.g. the one restored from EEPROM).
 * 'hooks' are passed through to the security module for persistence. */
void App_SM_init(SystemState* st, SystemMode startup_mode, const SecurityHooks* hooks);

/* Feed one event into the state machine. It mutates *st in place. After this
 * returns, the task layer reads *st and updates the actuators + display. */
void App_SM_handleEvent(SystemState* st, const AppEvent* ev);

/* Convert a SystemMode to a short label for the display. */
const char* App_SM_modeToString(SystemMode mode);

#endif /* APP_SM_H_ */
