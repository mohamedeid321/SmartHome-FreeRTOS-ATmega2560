/*
 * RTOS_Tasks.c
 *
 * Layer  : RTOS
 * Target : ATmega2560 + FreeRTOS
 * Purpose: All task bodies. Each bridges the pure APP layer to the HAL.
 *
 * Author : (Mohamed Eid)
 */

#include "RTOS_Tasks.h"      /* Task prototypes + priorities */
#include "RTOS_Shared.h"     /* Queues, mutexes, state access, logging */
#include "App_SM.h"          /* App_SM_modeToString */
#include "App_Fault.h"       /* App_Fault_toString */
#include "App_Control.h"     /* App_Control_fanSpeedPercent */
#include "SystemConfig.h"    /* Periods, pins */

/* HAL drivers the tasks drive. */
#include "LCD_DRIVER.h"
#include "KEYPAD_DRIVER.h"
#include "LM35_DRIVER.h"
#include "LDR_DRIVER.h"
#include "LED_DRIVER.h"
#include "RELAY_DRIVER.h"
#include "MOTOR_DRIVER.h"
#include "BUZZER_DRIVER.h"
#include "SD_CARD_DRIVER.h"

/* MCAL used directly by tasks. */
#include "UART_DRIVER.h"
/* NOTE: WDT_DRIVER is intentionally NOT included. miniAVRfreeRTOS uses the
 * Watchdog Timer as its tick source, so the WDT must not be touched here. */

/*------------------------------------------------------------------------
 * Handles for the device instances that use the "handle" style.
 * They are initialised in main() and referenced here.
 *----------------------------------------------------------------------*/
extern LED_Handle   g_statusLed;     /* Status / alarm LED */
extern RELAY_Handle g_lightRelay;    /* Room light relay */

/*------------------------------------------------------------------------
 * Helper: write one line to the UART terminal under the UART mutex.
 * Everything shown on the LCD is also mirrored here.
 *----------------------------------------------------------------------*/
static void uart_line(const char* s)
{
	if(xSemaphoreTake(g_uartMutex, portMAX_DELAY) == pdTRUE)   /* Lock the UART */
	{
		UART_sendString(CFG_UART_UNIT, s);                     /* Send the text */
		UART_sendString(CFG_UART_UNIT, "\r\n");                /* New line */
		xSemaphoreGive(g_uartMutex);                           /* Unlock */
	}
}

/*========================================================================
 * Task_Sensor : read LM35 + LDR once per second, post events.
 *======================================================================*/
void Task_Sensor(void* pv)
{
	(void)pv;                                  /* Unused parameter */
	TickType_t last = xTaskGetTickCount();     /* For precise periodic timing */

	for(;;)
	{
		AppEvent ev;

		/* --- temperature --- */
		uint16_t t = LM35_readC();             /* Whole-degree reading (HAL, averaged) */
		ev.type  = EV_TEMP_UPDATE;             /* Build a temperature event */
		ev.value = t;
		RTOS_applyEvent(&ev);                  /* Let the state machine react */

		/* --- light --- */
		uint8_t l = LDR_readPercent();         /* 0..100 % brightness */
		ev.type  = EV_LIGHT_UPDATE;            /* Build a light event */
		ev.value = l;
		RTOS_applyEvent(&ev);

		/* Run exactly once per CFG_PERIOD_SENSOR_MS (drift-free). */
		vTaskDelayUntil(&last, pdMS_TO_TICKS(CFG_PERIOD_SENSOR_MS));
	}
}

/*========================================================================
 * Task_Keypad : scan the keypad; on a key, post a key event to the SM.
 *======================================================================*/
void Task_Keypad(void* pv)
{
	(void)pv;
	for(;;)
	{
		uint8_t key = KEYPAD_getKey();         /* Non-blocking debounced scan */

		if(key != KEYPAD_NO_KEY)               /* A fresh key press? */
		{
			AppEvent ev;
			ev.type = EV_KEY;                  /* Build a key event */
			ev.key  = key;
			ev.value = 0;
			RTOS_applyEvent(&ev);              /* State machine handles it */
		}

		vTaskDelay(pdMS_TO_TICKS(CFG_PERIOD_KEYPAD_MS));   /* Re-scan after the debounce period */
	}
}

/*========================================================================
 * Task_Control : the heartbeat of the system. It periodically takes a state
 * snapshot and drives the physical actuators to match the APP's decisions.
 * (The state itself is mutated by the *_applyEvent calls in other tasks.)
 *======================================================================*/
void Task_Control(void* pv)
{
	(void)pv;
	TickType_t last = xTaskGetTickCount();

	for(;;)
	{
		SystemState st;
		RTOS_getState(&st);                    /* Safe snapshot of the decisions */

		/* --- drive the fan to match st.fan --- */
		uint8_t speed = App_Control_fanSpeedPercent(st.fan);   /* 0/50/100 */
		if(speed == 0)
		{
			MOTOR_stop();                      /* Fan off */
		}
		else
		{
			MOTOR_drive(MOTOR_FORWARD, speed); /* Fan forward at the chosen speed */
		}

		/* --- drive the light relay --- */
		if(st.light_on) RELAY_on(&g_lightRelay);   /* Lamp on */
		else            RELAY_off(&g_lightRelay);  /* Lamp off */

		/* --- drive the buzzer (over-temp alarm) --- */
		if(st.buzzer_on) BUZZER_on();          /* Sound the alarm */
		else             BUZZER_off();         /* Silence */

		vTaskDelayUntil(&last, pdMS_TO_TICKS(CFG_PERIOD_CONTROL_MS));
	}
}

/*========================================================================
 * Task_Display : render the SystemState to the LCD and mirror to UART.
 *======================================================================*/
void Task_Display(void* pv)
{
	(void)pv;
	for(;;)
	{
		SystemState st;
		RTOS_getState(&st);                    /* Snapshot to display */

		if(xSemaphoreTake(g_lcdMutex, portMAX_DELAY) == pdTRUE)   /* Lock the LCD */
		{
			LCD_clear();

			if(st.mode == MODE_SECURITY)
			{
				/* Security screens depend on the sub-state. */
				switch(st.sec_state)
				{
					case SEC_LOCKED:
					case SEC_ENTERING:
						LCD_gotoXY(0,0); LCD_printString("Enter Password:");
						LCD_gotoXY(1,0);
						for(uint8_t i=0;i<st.pw_index;i++) LCD_sendChar('*');  /* Masked digits */
						break;
					case SEC_RECOVERY:
						LCD_gotoXY(0,0); LCD_printString("Recovery Question");
						LCD_gotoXY(1,0); LCD_printString("Answer + Enter");
						break;
					case SEC_INTRUDER_LOCKOUT:
						LCD_gotoXY(0,0); LCD_printString("INTRUDER ALERT");
						LCD_gotoXY(1,0); LCD_printString("Wait: ");
						LCD_printNumber(st.lockout_remaining);                  /* Countdown */
						LCD_printString("s");
						break;
					case SEC_UNLOCKED:
						LCD_gotoXY(0,0); LCD_printString("Access Granted");
						break;
				}
			}
			else
			{
				/* AUTO / MANUAL : the standard status screen. */
				LCD_gotoXY(0,0); LCD_printString("TEMP: ");
				LCD_printNumber(st.temperature_c); LCD_printString("C");

				LCD_gotoXY(1,0); LCD_printString("LIGHT: ");
				LCD_printNumber(st.light_percent); LCD_printString("%");

				LCD_gotoXY(2,0); LCD_printString("MODE: ");
				LCD_printString(App_SM_modeToString(st.mode));

				LCD_gotoXY(3,0);
				if(st.fault == FAULT_OVER_TEMP) LCD_printString("** OVER TEMP **");
				else if(st.fault != FAULT_NONE) LCD_printString(App_Fault_toString(st.fault));
				else { LCD_printString("FAN:");
				       LCD_printNumber(App_Control_fanSpeedPercent(st.fan));
				       LCD_printString("% LIGHT:");
				       LCD_printString(st.light_on ? "ON" : "OFF"); }
			}
			xSemaphoreGive(g_lcdMutex);        /* Unlock the LCD */
		}

		/* Mirror a compact status line to the terminal. */
		{
			char line[40];
			/* Build "T=xx L=yy% MODE=zzz" without sprintf (lighter on AVR). */
			char* p = line;
			const char* m = App_SM_modeToString(st.mode);
			*p++='T'; *p++='='; 
			if(st.temperature_c>=10){*p++='0'+ (st.temperature_c/10)%10;} 
			*p++='0'+ st.temperature_c%10;
			*p++=' '; *p++='L'; *p++='=';
			if(st.light_percent>=10){*p++='0'+(st.light_percent/10)%10;}
			*p++='0'+ st.light_percent%10; *p++='%'; *p++=' ';
			while(*m) *p++=*m++;
			*p='\0';
			uart_line(line);
		}

		vTaskDelay(pdMS_TO_TICKS(CFG_PERIOD_DISPLAY_MS));
	}
}

/*========================================================================
 * Task_Logger : drain the log queue to the SD card and the terminal.
 *
 * SD-safe: the SD card is only written if it initialised correctly at boot
 * (g_sd_ready). In Proteus the card often does not respond, which would make
 * SD_writeBlock busy-wait forever and hang the system. When the card is not
 * ready we still log to the UART terminal, so logging always works.
 *======================================================================*/
extern uint8_t g_sd_ready;   /* Set in main() after SD_init: 1 = usable, 0 = absent/failed */

void Task_Logger(void* pv)
{
	(void)pv;
	uint32_t block = 1;                        /* Next SD block to write a record into */

	for(;;)
	{
		LogMessage msg;
		/* Block until a log line is available (no busy-wait). */
		if(xQueueReceive(g_logQueue, &msg, portMAX_DELAY) == pdTRUE)
		{
			/* --- terminal: "HH:MM:SS  TEXT" --- */
			char line[40];
			uint32_t s  = msg.timestamp_s;
			uint8_t  hh = (uint8_t)((s/3600)%100);
			uint8_t  mm = (uint8_t)((s/60)%60);
			uint8_t  ss = (uint8_t)(s%60);
			char* p = line;
			*p++='0'+hh/10; *p++='0'+hh%10; *p++=':';
			*p++='0'+mm/10; *p++='0'+mm%10; *p++=':';
			*p++='0'+ss/10; *p++='0'+ss%10; *p++=' '; *p++=' ';
			const char* t = msg.text;
			while(*t) *p++=*t++;
			*p='\0';
			uart_line(line);                   /* Mirror every log to the terminal (always works) */

			/* --- SD card: ONLY if the card is still usable --- */
			if(g_sd_ready)
			{
				if(xSemaphoreTake(g_spiMutex, portMAX_DELAY) == pdTRUE)
				{
					uint8_t buf[SD_BLOCK_SIZE];
					for(uint16_t i=0;i<SD_BLOCK_SIZE;i++) buf[i]=0;   /* Clear the block */
					uint16_t i=0;
					while(line[i] && i<SD_BLOCK_SIZE){ buf[i]=line[i]; i++; }  /* Copy the text */

					SD_Status w = SD_writeBlock(block, buf);
					if(w == SD_OK)
					{
						block++;                   /* Advance to the next block on success */
					}
					else
					{
						/* The write failed/timed out (e.g. the simulated card is
						 * not really responding). Disable further SD writes so we
						 * never stall again - UART logging continues regardless. */
						g_sd_ready = 0;
					}
					xSemaphoreGive(g_spiMutex);
				}
			}
			/* If the card is not ready we simply skip it - UART already logged. */
		}
	}
}

/*========================================================================
 * Task_Heartbeat : prove the system is alive by blinking the status LED.
 *
 * NOTE: this task no longer kicks the watchdog. miniAVRfreeRTOS uses the
 * Watchdog Timer as the FreeRTOS tick source, so the WDT is owned by the
 * kernel and must not be reset by application code. The LED blink alone is
 * the "system alive" indicator: if the scheduler ever stops switching tasks,
 * this LED freezes, which is the visible clue.
 *======================================================================*/
void Task_Heartbeat(void* pv)
{
	(void)pv;
	for(;;)
	{
		LED_toggle(&g_statusLed);              /* Visible "I'm alive" blink */
		vTaskDelay(pdMS_TO_TICKS(CFG_PERIOD_HEARTBEAT_MS));
	}
}

/*========================================================================
 * Task_Door : wait on the door semaphore (given by the ISR), then act.
 *======================================================================*/
void Task_Door(void* pv)
{
	(void)pv;
	for(;;)
	{
		/* Sleep until the door ISR gives the semaphore (zero CPU while waiting). */
		if(xSemaphoreTake(g_doorSem, portMAX_DELAY) == pdTRUE)
		{
			AppEvent ev;
			ev.type = EV_DOOR_OPENED;          /* Build a door event */
			ev.key = 0; ev.value = 0;
			RTOS_applyEvent(&ev);              /* Let the SM record it */

			RTOS_log("DOOR OPEN");             /* Log the event */
		}
	}
}