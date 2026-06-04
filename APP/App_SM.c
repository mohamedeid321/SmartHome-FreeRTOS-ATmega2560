/*
 * App_SM.c
 *
 * Layer  : APP
 * Target : ATmega2560
 * Purpose: The central state machine. Pure logic; mutates SystemState only.
 *
 * Author : (Mohamed Eid)
 */

#include "App_SM.h"          /* Our own prototypes */
#include "App_Control.h"     /* AUTO-mode decision rules */
#include "App_Security.h"    /* SECURITY sub-machine */
#include "App_Fault.h"       /* Fault classification */
#include "SystemConfig.h"    /* Thresholds and sizes */

/*------------------------------------------------------------------------
 * Helpers that apply the AUTO-mode rules to the live state.
 *----------------------------------------------------------------------*/
static void apply_auto_rules(SystemState* st)
{
	/* Fan from temperature. */
	st->fan = App_Control_decideFan(st->temperature_c);          /* OFF / MED / HIGH */

	/* Light from brightness. */
	st->light_on = App_Control_decideLight(st->light_percent);   /* 1 / 0 */

	/* Over-temperature alarm. */
	st->buzzer_on = App_Control_isOverTemp(st->temperature_c);   /* sound if too hot */
}

/*------------------------------------------------------------------------
 * Handle a keypad key while in MANUAL mode (the 1..5 command set).
 *----------------------------------------------------------------------*/
static void manual_handle_key(SystemState* st, uint8_t key)
{
	switch(key)
	{
		case '1': st->fan = FAN_HIGH; break;        /* 1 -> Fan ON (full) */
		case '2': st->fan = FAN_OFF;  break;        /* 2 -> Fan OFF */
		case '3': st->light_on = 1;   break;        /* 3 -> Light ON */
		case '4': st->light_on = 0;   break;        /* 4 -> Light OFF */
		case '5':                                   /* 5 -> switch to SECURITY */
			st->mode = MODE_SECURITY;
			App_Security_enter(st);                 /* Initialise the security sub-machine */
			break;
		default: break;                             /* Ignore other keys */
	}
}

/*------------------------------------------------------------------------
 * Top-level mode switching from dedicated keys (works in AUTO/MANUAL).
 * We reserve 'A' = AUTO, 'B' = MANUAL so the user can always get back.
 *----------------------------------------------------------------------*/
static uint8_t try_mode_switch(SystemState* st, uint8_t key)
{
	if(key == 'A') { st->mode = MODE_AUTO;   return 1; }   /* 'A' -> AUTO */
	if(key == 'B') { st->mode = MODE_MANUAL; return 1; }   /* 'B' -> MANUAL */
	return 0;                                              /* Not a mode key */
}

/*========================================================================
 * App_SM_init : set the starting mode and initialise sub-modules.
 *======================================================================*/
void App_SM_init(SystemState* st, SystemMode startup_mode, const SecurityHooks* hooks)
{
	/* Clear the whole state to a known baseline. */
	st->mode              = startup_mode;     /* Begin in the restored mode */
	st->sec_state         = SEC_LOCKED;
	st->temperature_c     = 0;
	st->light_percent     = 0;
	st->fan               = FAN_OFF;
	st->light_on          = 0;
	st->buzzer_on         = 0;
	st->pw_index          = 0;
	st->wrong_attempts    = 0;
	st->lockout_remaining = 0;
	st->door_open         = 0;
	st->fault             = FAULT_NONE;
	st->uptime_seconds    = 0;

	App_Security_init(hooks);                 /* Load the saved password + hooks */

	if(st->mode == MODE_SECURITY)             /* If we boot straight into security ... */
	{
		App_Security_enter(st);               /* ... start at the locked prompt */
	}
}

/*========================================================================
 * App_SM_handleEvent : the main dispatch.
 *======================================================================*/
void App_SM_handleEvent(SystemState* st, const AppEvent* ev)
{
	switch(ev->type)
	{
		/* ---- New sensor readings ---- */
		case EV_TEMP_UPDATE:
			st->temperature_c = ev->value;                       /* Store the reading */
			st->fault = App_Fault_checkTemp(ev->value);          /* Classify it (fault/over-temp/ok) */
			if(st->mode == MODE_AUTO) apply_auto_rules(st);      /* Re-run AUTO rules */
			break;

		case EV_LIGHT_UPDATE:
			st->light_percent = (uint8_t)ev->value;              /* Store the reading */
			{
				FaultCode lf = App_Fault_checkLight((uint8_t)ev->value);  /* Validate it */
				if(lf != FAULT_NONE) st->fault = lf;             /* Record a light fault if any */
			}
			if(st->mode == MODE_AUTO) apply_auto_rules(st);      /* Re-run AUTO rules */
			break;

		/* ---- Keypad ---- */
		case EV_KEY:
			/* Mode keys ('A'/'B') always work outside SECURITY entry. */
			if(st->mode != MODE_SECURITY)
			{
				if(try_mode_switch(st, ev->key)) break;          /* Consumed as a mode switch */
			}

			if(st->mode == MODE_MANUAL)
			{
				manual_handle_key(st, ev->key);                  /* 1..5 device commands */
			}
			else if(st->mode == MODE_SECURITY)
			{
				/* Feed the key into the security sub-machine. */
				SecurityState s = App_Security_handleKey(st, ev->key);
				if(s == SEC_UNLOCKED)
				{
					/* On unlock, drop back to AUTO as the default working mode. */
					st->mode = MODE_AUTO;
					apply_auto_rules(st);
				}
			}
			/* In AUTO, non-mode keys are ignored (system runs itself). */
			break;

		/* ---- Door sensor ISR ---- */
		case EV_DOOR_OPENED:
			st->door_open = 1;                                   /* Mark the door open */
			/* In SECURITY while locked, an open door is an event worth logging
			 * (handled by the task layer reading this flag). */
			break;

		/* ---- Recovery button ISR ---- */
		case EV_RECOVERY_BTN:
			if(st->mode == MODE_SECURITY)
			{
				App_Security_startRecovery(st);                  /* Begin the secret-question flow */
			}
			break;

		/* ---- One-second tick ---- */
		case EV_TICK_1S:
			st->uptime_seconds++;                                /* Advance our clock */
			App_Security_tick1s(st);                             /* Progress any lockout countdown */
			break;

		case EV_NONE:
		default:
			break;                                               /* Nothing to do */
	}
}

/*========================================================================
 * App_SM_modeToString : label for the display.
 *======================================================================*/
const char* App_SM_modeToString(SystemMode mode)
{
	switch(mode)
	{
		case MODE_AUTO:     return "AUTO";
		case MODE_MANUAL:   return "MANUAL";
		case MODE_SECURITY: return "SECURITY";
		default:            return "?";
	}
}
