/*
 * App_Security.h
 *
 * Layer  : APP (pure logic - no hardware)
 * Target : ATmega2560
 *
 * Password / access-control logic for SECURITY mode. These functions work
 * ONLY on the SystemState struct and a small persistence callback set; they
 * do not read the keypad or write EEPROM directly. The RTOS/security task
 * feeds them key presses and supplies load/save hooks. This keeps the
 * security rules testable and free of hardware coupling.
 *
 * Persistence hooks:
 *   The app does not know how EEPROM works; the task layer passes two
 *   function pointers - one to load the saved password, one to save a new
 *   one - so this module stays pure.
 *
 * Author : (Mohamed Eid)
 */

#ifndef APP_SECURITY_H_       /* Include guard opening */
#define APP_SECURITY_H_       /* Guard token */

#include <stdint.h>           /* Fixed-width integer types */
#include "App_Types.h"        /* SystemState, SecurityState */

/*------------------------------------------------------------------------
 * Persistence hooks the task layer provides (so this module stays pure).
 *----------------------------------------------------------------------*/
typedef struct {
	void (*load_password)(uint8_t* out, uint8_t len);        /* Read the saved password */
	void (*save_password)(const uint8_t* pw, uint8_t len);   /* Persist a new password */
	void (*save_lockout)(uint8_t locked);                    /* Persist the lockout flag */
}SecurityHooks;

/*================================ APIs ==================================*/

/* Initialise the security module with persistence hooks and load the
 * current password from storage. Call once at startup. */
void App_Security_init(const SecurityHooks* hooks);

/* Enter SECURITY mode: reset the entry buffer and go to SEC_LOCKED. */
void App_Security_enter(SystemState* st);

/* Feed one keypad key into the security state machine. Updates st->sec_state,
 * the password buffer, attempt counter, etc. Returns the (possibly new)
 * security state so the caller can react (e.g. log an event). */
SecurityState App_Security_handleKey(SystemState* st, uint8_t key);

/* Called once per second while locked out: decrements the countdown and
 * returns to SEC_LOCKED when it reaches zero. */
void App_Security_tick1s(SystemState* st);

/* Begin the recovery flow (secret question). */
void App_Security_startRecovery(SystemState* st);

/* Provide the recovery answer (1 char at a time or as a final check).
 * Returns 1 if the answer is correct (caller may then set a new password). */
uint8_t App_Security_checkRecoveryAnswer(const uint8_t* answer, uint8_t len);

/* Commit a brand-new password (used after a correct recovery answer). */
void App_Security_setNewPassword(SystemState* st, const uint8_t* pw, uint8_t len);

#endif /* APP_SECURITY_H_ */
