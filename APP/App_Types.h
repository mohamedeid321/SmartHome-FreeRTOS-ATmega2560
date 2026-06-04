/*
 * App_Types.h
 *
 * Layer  : APP (shared type definitions)
 * Target : ATmega2560
 *
 * This header is the common vocabulary of the application. Every other APP
 * file and every RTOS task includes it so they all speak about the same
 * modes, states, events and data structures. It contains TYPES ONLY - no
 * logic, no hardware access - which keeps it safe to include anywhere.
 *
 * Author : (Mohamed Eid)
 */

#ifndef APP_TYPES_H_          /* Include guard opening */
#define APP_TYPES_H_          /* Guard token */

#include <stdint.h>           /* Fixed-width integer types */
#include "SystemConfig.h"     /* For CFG_PASSWORD_LEN and other sizes */

/*========================================================================
 * SYSTEM MODES (top level of the state machine)
 *======================================================================*/
typedef enum {
	MODE_AUTO = 0,    /* System decides: auto light + fan control */
	MODE_MANUAL,      /* User drives devices from the keypad */
	MODE_SECURITY     /* Password-protected access control */
}SystemMode;

/*========================================================================
 * SECURITY SUB-STATES (active only while MODE_SECURITY)
 *======================================================================*/
typedef enum {
	SEC_LOCKED = 0,        /* Waiting at the "Enter password" prompt */
	SEC_ENTERING,          /* User is typing the password */
	SEC_UNLOCKED,          /* Correct password: access granted */
	SEC_RECOVERY,          /* "Forgot password": answering the secret question */
	SEC_INTRUDER_LOCKOUT   /* Too many wrong tries: counting down the lockout */
}SecurityState;

/*========================================================================
 * EVENTS fed into the state machine (from keypad, sensors, ISRs, timers)
 *======================================================================*/
typedef enum {
	EV_NONE = 0,        /* No event */
	EV_KEY,             /* A keypad key was pressed (see 'key' field) */
	EV_TEMP_UPDATE,     /* A new temperature reading is available */
	EV_LIGHT_UPDATE,    /* A new light reading is available */
	EV_DOOR_OPENED,     /* Door sensor fired (from ISR) */
	EV_RECOVERY_BTN,    /* "Forgot password" button pressed (from ISR) */
	EV_TICK_1S          /* One-second timer tick (for countdowns) */
}AppEventType;

/*------------------------------------------------------------------------
 * One event packet. Carries its type plus any payload it needs.
 *----------------------------------------------------------------------*/
typedef struct {
	AppEventType type;   /* What happened */
	uint8_t      key;    /* For EV_KEY: the character pressed */
	uint16_t     value;  /* For sensor events: the reading */
}AppEvent;

/*========================================================================
 * DEVICE / OUTPUT STATE (what the actuators should currently be doing)
 *======================================================================*/
typedef enum {
	FAN_OFF = 0,     /* Fan stopped */
	FAN_MED,         /* Fan at medium speed */
	FAN_HIGH         /* Fan at full speed */
}FanState;

/*========================================================================
 * FAULT CODES (set by the fault-detection logic)
 *======================================================================*/
typedef enum {
	FAULT_NONE = 0,        /* All readings sane */
	FAULT_TEMP_SENSOR,     /* Temperature out of physically plausible range */
	FAULT_LIGHT_SENSOR,    /* Light reading invalid */
	FAULT_SD_CARD,         /* SD logging failed */
	FAULT_OVER_TEMP        /* Genuine over-temperature condition */
}FaultCode;

/*========================================================================
 * SYSTEM STATE - the single struct that holds the whole live state.
 * The control task owns it; other tasks read snapshots via a mutex.
 *======================================================================*/
typedef struct {
	/* --- current modes --- */
	SystemMode    mode;                 /* AUTO / MANUAL / SECURITY */
	SecurityState sec_state;            /* sub-state when in SECURITY */

	/* --- latest sensor readings --- */
	uint16_t      temperature_c;        /* Temperature in whole degrees C */
	uint8_t       light_percent;        /* Light level 0..100 % */

	/* --- actuator states --- */
	FanState      fan;                  /* Current fan state */
	uint8_t       light_on;             /* 1 = lamp on, 0 = off */
	uint8_t       buzzer_on;            /* 1 = buzzer sounding */

	/* --- security working data --- */
	uint8_t       pw_buffer[CFG_PASSWORD_LEN];  /* Digits typed so far */
	uint8_t       pw_index;             /* How many digits entered */
	uint8_t       wrong_attempts;       /* Consecutive wrong tries */
	uint16_t      lockout_remaining;    /* Seconds left in intruder lockout */
	uint8_t       door_open;            /* 1 = door currently reported open */

	/* --- diagnostics --- */
	FaultCode     fault;                /* Most recent fault code */
	uint32_t      uptime_seconds;       /* Seconds since power-up (our clock) */
}SystemState;

#endif /* APP_TYPES_H_ */
