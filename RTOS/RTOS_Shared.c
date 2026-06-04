/*
 * RTOS_Shared.c
 *
 * Layer  : RTOS
 * Target : ATmega2560 + FreeRTOS
 * Purpose: Implementation of the shared kernel objects and safe state access.
 *
 * Author : (Mohamed Eid)
 */

#include "RTOS_Shared.h"     /* Our own prototypes + shared object declarations */
#include "App_SM.h"          /* App_SM_handleEvent runs under the state mutex */
#include "EEPROM_DRIVER.h"   /* Security hooks persist to internal EEPROM */
#include "SystemConfig.h"    /* EEPROM addresses, password length */
#include <string.h>          /* strncpy for log text */

/*------------------------------------------------------------------------
 * Definitions of the shared objects declared 'extern' in the header.
 *----------------------------------------------------------------------*/
QueueHandle_t     g_eventQueue = NULL;     /* Event queue (control task consumes) */
QueueHandle_t     g_logQueue   = NULL;     /* Log queue (logger task consumes) */
SemaphoreHandle_t g_lcdMutex   = NULL;     /* LCD mutex */
SemaphoreHandle_t g_spiMutex   = NULL;     /* SPI/SD mutex */
SemaphoreHandle_t g_uartMutex  = NULL;     /* UART mutex */
SemaphoreHandle_t g_stateMutex = NULL;     /* SystemState mutex */
SemaphoreHandle_t g_doorSem    = NULL;     /* Door ISR -> task semaphore */

/*------------------------------------------------------------------------
 * The one and only live SystemState. All access goes through the mutex.
 *----------------------------------------------------------------------*/
static SystemState g_state;

/*========================================================================
 * EEPROM-backed security persistence hooks (bind APP security to hardware).
 *======================================================================*/
static void hook_load_password(uint8_t* out, uint8_t len)
{
	uint8_t magic;
	EEPROM_readByte(CFG_EE_MAGIC_ADDR, &magic);     /* Has EEPROM ever been initialised? */

	if(magic != CFG_EE_MAGIC_VALUE)                 /* First-ever boot: no saved password */
	{
		/* Seed EEPROM with the factory default password and the magic marker. */
		out[0] = CFG_DEFAULT_PW0; out[1] = CFG_DEFAULT_PW1;
		out[2] = CFG_DEFAULT_PW2; out[3] = CFG_DEFAULT_PW3;
		EEPROM_writeBlock(CFG_EE_PASSWORD_ADDR, out, len);   /* Save the default */
		EEPROM_writeByte(CFG_EE_MAGIC_ADDR, CFG_EE_MAGIC_VALUE);  /* Mark as initialised */
	}
	else                                            /* Normal boot: load the saved password */
	{
		EEPROM_readBlock(CFG_EE_PASSWORD_ADDR, out, len);
	}
}

static void hook_save_password(const uint8_t* pw, uint8_t len)
{
	EEPROM_writeBlock(CFG_EE_PASSWORD_ADDR, pw, len);   /* Persist the new password */
}

static void hook_save_lockout(uint8_t locked)
{
	EEPROM_writeByte(CFG_EE_LOCKOUT_ADDR, locked);      /* Persist the lockout flag */
}

static const SecurityHooks g_securityHooks = {
	.load_password = hook_load_password,
	.save_password = hook_save_password,
	.save_lockout  = hook_save_lockout
};

const SecurityHooks* RTOS_getSecurityHooks(void)
{
	return &g_securityHooks;       /* Hand the hooks to App_SM_init */
}

/*========================================================================
 * RTOS_Shared_init : allocate every shared object.
 *======================================================================*/
uint8_t RTOS_Shared_init(void)
{
	/* Queues: depth chosen to absorb short bursts without dropping events. */
	g_eventQueue = xQueueCreate(10, sizeof(AppEvent));     /* 10 pending events */
	g_logQueue   = xQueueCreate(8,  sizeof(LogMessage));   /* 8 pending log lines */

	/* Mutexes for the shared peripherals and the state. */
	g_lcdMutex   = xSemaphoreCreateMutex();
	g_spiMutex   = xSemaphoreCreateMutex();
	g_uartMutex  = xSemaphoreCreateMutex();
	g_stateMutex = xSemaphoreCreateMutex();

	/* Binary semaphore the door ISR gives. */
	g_doorSem    = xSemaphoreCreateBinary();

	/* If any allocation failed, report it so main() can stop safely. */
	if(g_eventQueue == NULL || g_logQueue == NULL ||
	   g_lcdMutex == NULL || g_spiMutex == NULL || g_uartMutex == NULL ||
	   g_stateMutex == NULL || g_doorSem == NULL)
	{
		return 0;                  /* Out of heap: caller should halt */
	}
	return 1;                      /* All objects created */
}

/*========================================================================
 * RTOS_getState : copy the live state out under the mutex.
 *======================================================================*/
void RTOS_getState(SystemState* out)
{
	if(xSemaphoreTake(g_stateMutex, portMAX_DELAY) == pdTRUE)   /* Lock the state */
	{
		*out = g_state;                                        /* Snapshot copy */
		xSemaphoreGive(g_stateMutex);                          /* Unlock */
	}
}

/*========================================================================
 * RTOS_applyEvent : run the state machine for one event under the mutex.
 *======================================================================*/
void RTOS_applyEvent(const AppEvent* ev)
{
	if(xSemaphoreTake(g_stateMutex, portMAX_DELAY) == pdTRUE)   /* Lock the state */
	{
		App_SM_handleEvent(&g_state, ev);                      /* Mutate it safely */
		xSemaphoreGive(g_stateMutex);                          /* Unlock */
	}
}

/* Expose the live state pointer ONLY to App_SM_init at startup (before tasks
 * run, so no mutex is needed yet). Declared here, used by main. */
SystemState* RTOS_rawStateForInit(void)
{
	return &g_state;               /* Safe only before the scheduler starts */
}

/*========================================================================
 * RTOS_postEvent : push an event onto the control queue (task context).
 *======================================================================*/
void RTOS_postEvent(const AppEvent* ev)
{
	if(g_eventQueue != NULL)
	{
		xQueueSend(g_eventQueue, ev, 0);   /* Non-blocking: drop if full rather than stall */
	}
}

/*========================================================================
 * RTOS_postEventFromISR : push an event from an ISR.
 *======================================================================*/
void RTOS_postEventFromISR(const AppEvent* ev, BaseType_t* hpw)
{
	if(g_eventQueue != NULL)
	{
		xQueueSendFromISR(g_eventQueue, ev, hpw);   /* ISR-safe send */
	}
}

/*========================================================================
 * RTOS_log : build a timestamped log line and queue it for the logger.
 *======================================================================*/
void RTOS_log(const char* text)
{
	LogMessage msg;
	SystemState snap;

	RTOS_getState(&snap);                          /* Read the current uptime safely */
	msg.timestamp_s = snap.uptime_seconds;         /* Stamp the message */
	strncpy(msg.text, text, sizeof(msg.text) - 1); /* Copy the text (bounded) */
	msg.text[sizeof(msg.text) - 1] = '\0';         /* Guarantee termination */

	if(g_logQueue != NULL)
	{
		xQueueSend(g_logQueue, &msg, 0);           /* Non-blocking enqueue */
	}
}
