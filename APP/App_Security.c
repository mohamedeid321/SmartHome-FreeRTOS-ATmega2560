/*
 * App_Security.c
 *
 * Layer  : APP
 * Target : ATmega2560
 * Purpose: SECURITY-mode password logic (pure; persistence via hooks).
 *
 * Author : (Mohamed Eid)
 */

#include "App_Security.h"    /* Our own prototypes */
#include "SystemConfig.h"    /* CFG_PASSWORD_LEN, CFG_MAX_ATTEMPTS, CFG_LOCKOUT_SECONDS */

/*------------------------------------------------------------------------
 * Module-private storage: the active password and the persistence hooks.
 *----------------------------------------------------------------------*/
static uint8_t       g_password[CFG_PASSWORD_LEN];   /* The current correct password */
static SecurityHooks g_hooks = {0, 0, 0};            /* Load/save callbacks from the task layer */

/* A simple fixed recovery answer for the secret question (stored in EEPROM
 * in a full build; kept here as the default for clarity). */
static const uint8_t RECOVERY_ANSWER[] = { 'O','P','E','N' };   /* Example answer */

/*------------------------------------------------------------------------
 * Helper: compare the typed buffer against the stored password.
 *----------------------------------------------------------------------*/
static uint8_t passwords_match(const uint8_t* a, const uint8_t* b, uint8_t len)
{
	for(uint8_t i = 0; i < len; i++)     /* Compare every digit ... */
	{
		if(a[i] != b[i]) return 0;       /* ... any mismatch fails immediately */
	}
	return 1;                            /* All digits matched */
}

/*------------------------------------------------------------------------
 * Helper: clear the entry buffer so the next attempt starts fresh.
 *----------------------------------------------------------------------*/
static void reset_entry(SystemState* st)
{
	st->pw_index = 0;                    /* No digits entered yet */
	for(uint8_t i = 0; i < CFG_PASSWORD_LEN; i++)
	{
		st->pw_buffer[i] = 0;            /* Wipe the buffer */
	}
}

/*========================================================================
 * App_Security_init : store hooks and load the saved password.
 *======================================================================*/
void App_Security_init(const SecurityHooks* hooks)
{
	if(hooks != 0)                       /* If hooks were supplied ... */
	{
		g_hooks = *hooks;                /* ... keep a copy */
	}

	if(g_hooks.load_password != 0)       /* If we can load from storage ... */
	{
		g_hooks.load_password(g_password, CFG_PASSWORD_LEN);   /* ... load the saved password */
	}
	else                                 /* Otherwise use the factory default */
	{
		g_password[0] = CFG_DEFAULT_PW0;
		g_password[1] = CFG_DEFAULT_PW1;
		g_password[2] = CFG_DEFAULT_PW2;
		g_password[3] = CFG_DEFAULT_PW3;
	}
}

/*========================================================================
 * App_Security_enter : start at the locked prompt.
 *======================================================================*/
void App_Security_enter(SystemState* st)
{
	reset_entry(st);                     /* Clear any half-typed password */
	/* If a lockout was persisted across a reset, honour it; else lock normally. */
	if(st->lockout_remaining > 0)
	{
		st->sec_state = SEC_INTRUDER_LOCKOUT;   /* Resume the countdown */
	}
	else
	{
		st->sec_state = SEC_LOCKED;             /* Normal locked prompt */
	}
}

/*========================================================================
 * App_Security_handleKey : main password entry state machine.
 *======================================================================*/
SecurityState App_Security_handleKey(SystemState* st, uint8_t key)
{
	/* While locked out, ignore digit entry; only the recovery button (handled
	 * elsewhere) or the countdown can leave this state. */
	if(st->sec_state == SEC_INTRUDER_LOCKOUT)
	{
		return st->sec_state;            /* Keep waiting out the lockout */
	}

	/* Moving from LOCKED to ENTERING on the first key press. */
	if(st->sec_state == SEC_LOCKED)
	{
		st->sec_state = SEC_ENTERING;    /* Begin entry */
		reset_entry(st);                 /* Fresh buffer */
	}

	if(st->sec_state == SEC_ENTERING)
	{
		/* Store the digit if there is room. */
		if(st->pw_index < CFG_PASSWORD_LEN)
		{
			st->pw_buffer[st->pw_index] = key;   /* Save the pressed digit */
			st->pw_index++;                      /* Advance the position */
		}

		/* Once a full password has been typed, check it. */
		if(st->pw_index >= CFG_PASSWORD_LEN)
		{
			if(passwords_match(st->pw_buffer, g_password, CFG_PASSWORD_LEN))
			{
				/* Correct: grant access and clear the attempt counter. */
				st->wrong_attempts = 0;          /* Reset attempts on success */
				st->sec_state = SEC_UNLOCKED;    /* Access granted */
			}
			else
			{
				/* Wrong: count the failed attempt. */
				st->wrong_attempts++;            /* One more wrong try */
				reset_entry(st);                 /* Clear for a retry */

				if(st->wrong_attempts >= CFG_MAX_ATTEMPTS)
				{
					/* Too many: enter the intruder lockout and persist it. */
					st->sec_state = SEC_INTRUDER_LOCKOUT;          /* Lock out */
					st->lockout_remaining = CFG_LOCKOUT_SECONDS;   /* Start the countdown */
					if(g_hooks.save_lockout) g_hooks.save_lockout(1);  /* Survive a reset */
				}
				else
				{
					st->sec_state = SEC_LOCKED;  /* Back to the prompt for another try */
				}
			}
		}
	}

	return st->sec_state;                /* Tell the caller the new state */
}

/*========================================================================
 * App_Security_tick1s : count down the intruder lockout.
 *======================================================================*/
void App_Security_tick1s(SystemState* st)
{
	if(st->sec_state == SEC_INTRUDER_LOCKOUT && st->lockout_remaining > 0)
	{
		st->lockout_remaining--;         /* One second elapsed */

		if(st->lockout_remaining == 0)   /* Countdown finished? */
		{
			st->wrong_attempts = 0;                          /* Forgive the attempts */
			st->sec_state = SEC_LOCKED;                      /* Allow entry again */
			if(g_hooks.save_lockout) g_hooks.save_lockout(0);/* Clear the persisted flag */
		}
	}
}

/*========================================================================
 * App_Security_startRecovery : enter the secret-question flow.
 *======================================================================*/
void App_Security_startRecovery(SystemState* st)
{
	reset_entry(st);                     /* Clear the entry buffer */
	st->sec_state = SEC_RECOVERY;        /* Switch to recovery */
}

/*========================================================================
 * App_Security_checkRecoveryAnswer : verify the secret answer.
 *======================================================================*/
uint8_t App_Security_checkRecoveryAnswer(const uint8_t* answer, uint8_t len)
{
	if(len != sizeof(RECOVERY_ANSWER)) return 0;   /* Length must match */
	for(uint8_t i = 0; i < len; i++)               /* Compare each character */
	{
		if(answer[i] != RECOVERY_ANSWER[i]) return 0;   /* Mismatch -> wrong */
	}
	return 1;                                      /* Correct answer */
}

/*========================================================================
 * App_Security_setNewPassword : commit a new password after recovery.
 *======================================================================*/
void App_Security_setNewPassword(SystemState* st, const uint8_t* pw, uint8_t len)
{
	if(len != CFG_PASSWORD_LEN) return;            /* Must be the right length */

	for(uint8_t i = 0; i < CFG_PASSWORD_LEN; i++)  /* Copy the new password ... */
	{
		g_password[i] = pw[i];                     /* ... into the active password */
	}
	if(g_hooks.save_password)                      /* If we can persist it ... */
	{
		g_hooks.save_password(g_password, CFG_PASSWORD_LEN);   /* ... save to EEPROM */
	}

	/* Reset the security state to a clean locked prompt. */
	st->wrong_attempts    = 0;
	st->lockout_remaining = 0;
	reset_entry(st);
	st->sec_state = SEC_LOCKED;
}
