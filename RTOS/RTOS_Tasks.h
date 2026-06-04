/*
 * RTOS_Tasks.h
 *
 * Layer  : RTOS (the task functions)
 * Target : ATmega2560 + FreeRTOS
 *
 * Each task is a thin loop: it reads inputs or waits on a queue/semaphore,
 * turns that into an AppEvent, asks the APP state machine to process it
 * (via RTOS_applyEvent), then drives the actuators / display from the
 * resulting SystemState. The DECISIONS live in APP; the SCHEDULING lives
 * here. That separation is the whole point of the architecture.
 *
 * Author : (Mohamed Eid)
 */

#ifndef RTOS_TASKS_H_         /* Include guard opening */
#define RTOS_TASKS_H_         /* Guard token */

#include "FreeRTOS.h"         /* TaskFunction signatures */
#include "task.h"

/*------------------------------------------------------------------------
 * Task priorities (0 = idle .. 4 = highest, per configMAX_PRIORITIES=5).
 * Higher number = more urgent. Control + Security are highest so the system
 * reacts instantly; Display/Logger are lowest because they can lag a little.
 *----------------------------------------------------------------------*/
#define PRIO_HEARTBEAT   1    /* Blink LED + kick watchdog */
#define PRIO_DISPLAY     1    /* Refresh LCD + mirror to UART */
#define PRIO_LOGGER      1    /* Write logs to SD + UART */
#define PRIO_SENSOR      2    /* Read LM35 + LDR */
#define PRIO_KEYPAD      2    /* Scan the keypad */
#define PRIO_CONTROL     3    /* Run the state machine + drive actuators */
#define PRIO_DOOR        3    /* React to the door ISR semaphore */

/*------------------------------------------------------------------------
 * Stack sizes (in WORDS). The 2560 has room; logger/display touch strings
 * and the SD buffer, so they get a little more.
 *----------------------------------------------------------------------*/
#define STACK_HEARTBEAT  100
#define STACK_SENSOR     150
#define STACK_KEYPAD     150
#define STACK_CONTROL    200
#define STACK_DISPLAY    200
#define STACK_LOGGER     255
#define STACK_DOOR       150

/*================================ Task functions =======================*/
void Task_Sensor(void* pv);      /* Periodically read sensors -> events */
void Task_Keypad(void* pv);      /* Periodically scan keypad -> events */
void Task_Control(void* pv);     /* Consume events, run SM, drive outputs */
void Task_Display(void* pv);     /* Render SystemState to LCD + UART */
void Task_Logger(void* pv);      /* Drain the log queue to SD + UART */
void Task_Heartbeat(void* pv);   /* Blink LED + reset the watchdog */
void Task_Door(void* pv);        /* Wait on door semaphore -> event + log */

#endif /* RTOS_TASKS_H_ */
