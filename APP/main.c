/*
 * main.c
 *
 * Layer  : APP entry point
 * Target : ATmega2560 @ 16MHz + FreeRTOS
 * Project: Smart Home control system
 *
 * Boot sequence:
 *   1. Initialise every MCAL + HAL driver from SystemConfig.h.
 *   2. Restore the last system mode (and lockout flag) from EEPROM.
 *   3. Initialise the APP state machine with the EEPROM-backed hooks.
 *   4. Create the shared RTOS objects and all tasks.
 *   5. Start the watchdog and the scheduler.
 *
 * The ISRs (door + recovery button) live here and use FromISR APIs to wake
 * their tasks safely.
 *
 * Author : (Mohamed Eid)
 */

#include <avr/io.h>
#include <avr/interrupt.h>

#include "FreeRTOS.h"
#include "task.h"

/* CONFIG */
#include "SystemConfig.h"

/* MCAL */
#include "DIO_DRIVER.h"
#include "ADC_DRIVER.h"
#include "UART_DRIVER.h"
#include "SPI_DRIVER.h"
#include "EXT_INT_DRIVER.h"
#include "EEPROM_DRIVER.h"
/* NOTE: WDT_DRIVER is intentionally NOT included. miniAVRfreeRTOS uses the
 * Watchdog Timer as the FreeRTOS tick source, so we must not configure it here. */

/* HAL */
#include "LCD_DRIVER.h"
#include "KEYPAD_DRIVER.h"
#include "LM35_DRIVER.h"
#include "LDR_DRIVER.h"
#include "LED_DRIVER.h"
#include "RELAY_DRIVER.h"
#include "MOTOR_DRIVER.h"
#include "BUZZER_DRIVER.h"
#include "SD_CARD_DRIVER.h"

/* APP + RTOS */
#include "App_SM.h"
#include "RTOS_Shared.h"
#include "RTOS_Tasks.h"

/*------------------------------------------------------------------------
 * Device handles that use the "handle" style (shared with RTOS_Tasks.c).
 *----------------------------------------------------------------------*/
LED_Handle   g_statusLed;        /* Status / alarm LED */
RELAY_Handle g_lightRelay;       /* Room light relay */

/*========================================================================
 * ISR : door sensor (INT0). Wakes Task_Door via the binary semaphore.
 *======================================================================*/
static void door_isr_callback(void)
{
	BaseType_t hpw = pdFALSE;                          /* "higher priority task woken" flag */
	xSemaphoreGiveFromISR(g_doorSem, &hpw);            /* Wake Task_Door */
	//portYIELD_FROM_ISR(hpw);                           /* Switch to it immediately if higher prio */
}

/*========================================================================
 * ISR : recovery button (INT1). Posts a recovery event to the control queue.
 *======================================================================*/
static void recovery_isr_callback(void)
{
	BaseType_t hpw = pdFALSE;
	AppEvent ev;
	ev.type = EV_RECOVERY_BTN;                         /* Build the recovery event */
	ev.key = 0; ev.value = 0;
	RTOS_postEventFromISR(&ev, &hpw);                  /* ISR-safe post (consumed elsewhere) */
	//portYIELD_FROM_ISR(hpw);
}

/*------------------------------------------------------------------------
 * Initialise every driver from the central configuration.
 *----------------------------------------------------------------------*/
static void hardware_init(void)
{
	/* ---- UART terminal ---- */
	UART_Config uc;
	uc.m_unit = CFG_UART_UNIT; uc.m_baud = CFG_UART_BAUD;
	uc.m_data_bits = UART_DATA_8BIT; uc.m_parity = UART_PARITY_NONE;
	uc.m_mode = UART_MODE_TX_RX_POLLING; uc.callback = 0;
	UART_init(&uc);

	/* ---- ADC (AVCC reference, /128) ---- */
	ADC_CONFIG_SELCECTION ac;
	ac.m_channel = CFG_LM35_ADC_CH; ac.m_mode = ADC_MODE_POLLING;
	ac.m_vref = ADC_VREF_AVCC; ac.m_pre = ADC_PRESCALER_128; ac.callback = 0;
	ADC_init(&ac);

	/* ---- EEPROM ---- */
	EEPROM_init(0);                                    /* Default atomic mode */

	/* ---- LCD 20x4 ---- */
	LCD_Config lc;
	lc.m_data_port = CFG_LCD_DATA_PORT;
	lc.m_rs_port = CFG_LCD_RS_PORT; lc.m_rs_pin = CFG_LCD_RS_PIN;
	lc.m_rw_port = CFG_LCD_RW_PORT; lc.m_rw_pin = CFG_LCD_RW_PIN;
	lc.m_en_port = CFG_LCD_EN_PORT; lc.m_en_pin = CFG_LCD_EN_PIN;
	LCD_init(&lc);
	LCD_clear();
	LCD_gotoXY(0,0);
	LCD_printString("LCD TEST OK");

	/* ---- Keypad 4x4 ---- */
	KEYPAD_Config kc;
	kc.m_row_port[0]=CFG_KP_ROW_PORT; kc.m_row_pin[0]=CFG_KP_ROW0_PIN;
	kc.m_row_port[1]=CFG_KP_ROW_PORT; kc.m_row_pin[1]=CFG_KP_ROW1_PIN;
	kc.m_row_port[2]=CFG_KP_ROW_PORT; kc.m_row_pin[2]=CFG_KP_ROW2_PIN;
	kc.m_row_port[3]=CFG_KP_ROW_PORT; kc.m_row_pin[3]=CFG_KP_ROW3_PIN;
	kc.m_col_port[0]=CFG_KP_COL_PORT; kc.m_col_pin[0]=CFG_KP_COL0_PIN;
	kc.m_col_port[1]=CFG_KP_COL_PORT; kc.m_col_pin[1]=CFG_KP_COL1_PIN;
	kc.m_col_port[2]=CFG_KP_COL_PORT; kc.m_col_pin[2]=CFG_KP_COL2_PIN;
	kc.m_col_port[3]=CFG_KP_COL_PORT; kc.m_col_pin[3]=CFG_KP_COL3_PIN;
	KEYPAD_init(&kc);

	/* ---- Sensors ---- */
	LM35_Config tc; tc.m_channel = CFG_LM35_ADC_CH; LM35_init(&tc);
	LDR_Config  dc; dc.m_channel = CFG_LDR_ADC_CH; dc.m_polarity = LDR_BRIGHT_IS_HIGH; LDR_init(&dc);

	/* ---- Status LED ---- */
	LED_Handle led_cfg;
	led_cfg.m_port = CFG_STATUS_LED_PORT; led_cfg.m_pin = CFG_STATUS_LED_PIN;
	led_cfg.m_polarity = LED_ACTIVE_HIGH;
	LED_init(&g_statusLed, &led_cfg);

	/* ---- Light relay ---- */
	RELAY_Handle relay_cfg;
	relay_cfg.m_port = CFG_LIGHT_RELAY_PORT; relay_cfg.m_pin = CFG_LIGHT_RELAY_PIN;
	relay_cfg.m_polarity = RELAY_ACTIVE_LOW; relay_cfg._state = RELAY_OFF;
	RELAY_init(&g_lightRelay, &relay_cfg);

	/* ---- Fan motor (H-bridge + PWM) ---- */
	MOTOR_Config mc;
	mc.m_in1_port = CFG_FAN_IN1_PORT; mc.m_in1_pin = CFG_FAN_IN1_PIN;
	mc.m_in2_port = CFG_FAN_IN2_PORT; mc.m_in2_pin = CFG_FAN_IN2_PIN;
	mc.m_pwm_timer = CFG_FAN_PWM_TIMER;
	MOTOR_init(&mc);

	/* ---- Buzzer (active) ---- */
	BUZZER_Config bc;
	bc.m_type = BUZZER_ACTIVE; bc.m_port = CFG_BUZZER_PORT; bc.m_pin = CFG_BUZZER_PIN;
	bc.m_polarity = BUZZER_ACTIVE_HIGH; bc.m_timer = Timer2;
	BUZZER_init(&bc);

	/* ---- SD card over SPI ---- */
	SPI_init(0);                                       /* Default master config */
	SD_Config sc; sc.m_cs_port = CFG_SD_CS_PORT; sc.m_cs_pin = CFG_SD_CS_PIN;
	SD_init(&sc);                                      /* Bring the card up (best-effort) */

	/* ---- External interrupts: door + recovery button ---- */
	EXT_INT_CONFIG dxi;
	dxi.m_line = CFG_DOOR_EXT_LINE; dxi.m_sense = EXT_INT_FALLING_EDGE;
	dxi.m_pull = EXT_INT_PULLUP; dxi.callback = door_isr_callback;
	EXT_INT_init(&dxi);

	EXT_INT_CONFIG rxi;
	rxi.m_line = CFG_RECOVERY_EXT_LINE; rxi.m_sense = EXT_INT_FALLING_EDGE;
	rxi.m_pull = EXT_INT_PULLUP; rxi.callback = recovery_isr_callback;
	EXT_INT_init(&rxi);
}

/*------------------------------------------------------------------------
 * Restore the last mode + lockout flag from EEPROM.
 *----------------------------------------------------------------------*/
static SystemMode restore_mode(SystemState* st)
{
	uint8_t magic, lastmode, lockout;
	EEPROM_readByte(CFG_EE_MAGIC_ADDR, &magic);        /* Initialised marker */

	if(magic != CFG_EE_MAGIC_VALUE)                    /* Very first boot */
	{
		return MODE_AUTO;                              /* Default to AUTO */
	}

	EEPROM_readByte(CFG_EE_LASTMODE_ADDR, &lastmode);  /* Last saved mode */
	EEPROM_readByte(CFG_EE_LOCKOUT_ADDR, &lockout);    /* Persisted lockout? */

	if(lockout == 1)                                   /* Was the system locked when power cut? */
	{
		st->lockout_remaining = CFG_LOCKOUT_SECONDS;   /* Resume the lockout */
	}

	if(lastmode <= MODE_SECURITY) return (SystemMode)lastmode;   /* Valid stored mode */
	return MODE_AUTO;                                  /* Fallback */
}

/*========================================================================
 * main
 *======================================================================*/
int main(void)
{
	/* 1. Bring up all hardware. */
	hardware_init();
	

	/* 2. Create the shared RTOS objects; halt visibly if out of heap. */
	if(RTOS_Shared_init() == 0)
	{
		LCD_clear(); LCD_printString("RTOS init FAILED");   /* Show the failure */
		for(;;);                                            /* Stop here */
	}

	/* 3. Restore state + initialise the APP state machine. */
	SystemState* st = RTOS_rawStateForInit();              /* Safe: scheduler not started yet */
	SystemMode start = restore_mode(st);                   /* Last mode from EEPROM */
	App_SM_init(st, start, RTOS_getSecurityHooks());       /* Wire security to EEPROM */

	/* 4. Create all tasks. */
	xTaskCreate(Task_Sensor,    "SENS", STACK_SENSOR,    NULL, PRIO_SENSOR,    NULL);
	xTaskCreate(Task_Keypad,    "KEYP", STACK_KEYPAD,    NULL, PRIO_KEYPAD,    NULL);
	xTaskCreate(Task_Control,   "CTRL", STACK_CONTROL,   NULL, PRIO_CONTROL,   NULL);
	xTaskCreate(Task_Display,   "DISP", STACK_DISPLAY,   NULL, PRIO_DISPLAY,   NULL);
	//xTaskCreate(Task_Logger,    "LOGR", STACK_LOGGER,    NULL, PRIO_LOGGER,    NULL);
	xTaskCreate(Task_Heartbeat, "BEAT", STACK_HEARTBEAT, NULL, PRIO_HEARTBEAT, NULL);
	xTaskCreate(Task_Door,      "DOOR", STACK_DOOR,      NULL, PRIO_DOOR,      NULL);

	/* 5. Start the scheduler.
	 * NOTE: no manual watchdog setup here. miniAVRfreeRTOS configures the
	 * Watchdog Timer itself and uses it as the system tick. The Heartbeat
	 * task blinks the status LED as the "alive" indicator. */
	vTaskStartScheduler();                                 /* Hand control to FreeRTOS */

	/* Never reached unless the kernel could not start (no heap for idle task). */
	for(;;);
	return 0;
}

/*========================================================================
 * FreeRTOS diagnostic hooks (enabled in FreeRTOSConfig.h).
 *======================================================================*/
void vApplicationStackOverflowHook(TaskHandle_t xTask, char* pcTaskName)
{
	(void)xTask; (void)pcTaskName;
	/* A task overflowed its stack: stop so the watchdog resets the board. */
	for(;;);
}

void vApplicationMallocFailedHook(void)
{
	/* The heap is exhausted: stop so the watchdog resets the board. */
	for(;;);
}
