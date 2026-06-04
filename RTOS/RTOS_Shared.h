/*
 * RTOS_Shared.h
 *
 * Layer  : RTOS (shared kernel objects + safe state access)
 * Target : ATmega2560 + FreeRTOS
 *
 * This module owns the FreeRTOS objects that tasks share:
 *   - Queues   : carry events between tasks (sensor readings, key presses, logs).
 *   - Mutexes  : protect shared peripherals (LCD, SPI/SD, UART) and the
 *                global SystemState from concurrent access.
 *   - Semaphore: a binary semaphore an ISR gives to wake a waiting task.
 *
 * It also exposes thread-safe helpers to read/update the single SystemState
 * struct, so tasks never touch it without holding the mutex.
 *
 * Author : (Mohamed Eid)
 */

#ifndef RTOS_SHARED_H_        /* Include guard opening */
#define RTOS_SHARED_H_        /* Guard token */

#include "FreeRTOS.h"         /* Core FreeRTOS types */
#include "task.h"             /* Task API */
#include "queue.h"            /* Queue API */
#include "semphr.h"           /* Semaphore / mutex API */

#include "App_Types.h"        /* AppEvent, SystemState */
#include "App_Security.h"     /* SecurityHooks */

/*------------------------------------------------------------------------
 * Log message passed from any task to the logger task.
 *----------------------------------------------------------------------*/
typedef struct {
	uint32_t timestamp_s;     /* Uptime seconds when the event happened */
	char     text[24];        /* Short event description (e.g. "DOOR OPEN") */
}LogMessage;

/*------------------------------------------------------------------------
 * Global shared kernel objects (defined in RTOS_Shared.c).
 *----------------------------------------------------------------------*/
extern QueueHandle_t      g_eventQueue;    /* AppEvent: keypad/sensor/ISR -> control task */
extern QueueHandle_t      g_logQueue;      /* LogMessage: any task -> logger task */
extern SemaphoreHandle_t  g_lcdMutex;      /* Protects the LCD */
extern SemaphoreHandle_t  g_spiMutex;      /* Protects the SPI bus / SD card */
extern SemaphoreHandle_t  g_uartMutex;     /* Protects the UART terminal */
extern SemaphoreHandle_t  g_stateMutex;    /* Protects the global SystemState */
extern SemaphoreHandle_t  g_doorSem;       /* Given by the door ISR */

/*================================ APIs ==================================*/

/* Create every shared queue/mutex/semaphore. Call once before tasks start.
 * Returns 1 on success, 0 if any object could not be allocated. */
uint8_t RTOS_Shared_init(void);

/* Thread-safe: copy the current SystemState into *out (takes g_stateMutex). */
void RTOS_getState(SystemState* out);

/* Thread-safe: run the state machine for one event under the state mutex.
 * This is how every task mutates the system - never directly. */
void RTOS_applyEvent(const AppEvent* ev);

/* Post an event to the control task's event queue (from a task context). */
void RTOS_postEvent(const AppEvent* ev);

/* Post an event from an ISR context (uses the FromISR API). */
void RTOS_postEventFromISR(const AppEvent* ev, BaseType_t* hpw);

/* Queue a log line (from a task). Builds the LogMessage with the current uptime. */
void RTOS_log(const char* text);

/* Get the SecurityHooks that bind the APP's security module to the EEPROM. */
const SecurityHooks* RTOS_getSecurityHooks(void);

#endif /* RTOS_SHARED_H_ */
