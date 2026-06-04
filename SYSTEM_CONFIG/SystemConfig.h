/*
 * SystemConfig.h
 *
 * Layer  : CONFIG (single source of truth for the whole project)
 * Target : ATmega2560 @ 16MHz
 *
 * Everything you might want to tweak - which pin each device uses, the ADC
 * channels, the control thresholds, the security settings - lives here. No
 * driver or task file hard-codes a pin or a magic number; they all read from
 * this file. To re-wire the board or re-tune the logic, you edit ONLY this.
 *
 * Sections:
 *   1. Clock
 *   2. MCAL channel / unit selection
 *   3. HAL device pin mapping
 *   4. Control thresholds (Auto mode)
 *   5. Security settings
 *   6. Timing / task periods
 *   7. EEPROM memory map
 *
 * Author : (Mohamed Eid)
 */

#ifndef SYSTEM_CONFIG_H_       /* Include guard opening */
#define SYSTEM_CONFIG_H_       /* Guard token */

/* Pull in the enums used by the configs below (ports, pins, channels...). */
#include "DIO_DRIVER.h"        /* DIO_PORTx / DIO_PINx */
#include "ADC_DRIVER.h"        /* ADC_CHANNEL_x */
#include "UART_DRIVER.h"       /* UART_UNIT_x / baud */
#include "TIMER_DRIVER.h"      /* TIMER ids */
#include "EXT_INT_DRIVER.h"    /* EXT_INTx lines */

/*========================================================================
 * 1. CLOCK
 *======================================================================*/
#ifndef F_CPU
#define F_CPU                 16000000UL   /* 16 MHz board clock (set also in project Symbols) */
#endif

/*========================================================================
 * 2. MCAL CHANNEL / UNIT SELECTION
 *======================================================================*/
/* Which USART talks to the PC terminal (logging / fault output). */
#define CFG_UART_UNIT         UART_UNIT_0       /* USART0 = the USB/programmer port */
#define CFG_UART_BAUD         UART_BAUD_9600    /* Terminal baud rate */

/* ADC channels for the analog sensors (keep them clear of LCD control pins). */
#define CFG_LM35_ADC_CH       ADC_CHANNEL_3     /* LM35 temperature on ADC3 (PF3) */
#define CFG_LDR_ADC_CH        ADC_CHANNEL_4     /* LDR light on ADC4 (PF4) */

/*========================================================================
 * 3. HAL DEVICE PIN MAPPING
 *======================================================================*/
/* ---- LCD 20x4 (4-bit). Data nibble on PORTC PC4..PC7; control on PORTA. */
#define CFG_LCD_DATA_PORT     DIO_PORTC
#define CFG_LCD_RS_PORT       DIO_PORTA
#define CFG_LCD_RS_PIN        DIO_PIN2
#define CFG_LCD_RW_PORT       DIO_PORTA
#define CFG_LCD_RW_PIN        DIO_PIN1
#define CFG_LCD_EN_PORT       DIO_PORTA
#define CFG_LCD_EN_PIN        DIO_PIN0

/* ---- Keypad 4x4 on PORTL (rows PL0..PL3 out, cols PL4..PL7 in). */
#define CFG_KP_ROW_PORT       DIO_PORTL
#define CFG_KP_COL_PORT       DIO_PORTL
#define CFG_KP_ROW0_PIN       DIO_PIN0
#define CFG_KP_ROW1_PIN       DIO_PIN1
#define CFG_KP_ROW2_PIN       DIO_PIN2
#define CFG_KP_ROW3_PIN       DIO_PIN3
#define CFG_KP_COL0_PIN       DIO_PIN4
#define CFG_KP_COL1_PIN       DIO_PIN5
#define CFG_KP_COL2_PIN       DIO_PIN6
#define CFG_KP_COL3_PIN       DIO_PIN7

/* ---- Relay controlling the room light. */
#define CFG_LIGHT_RELAY_PORT  DIO_PORTA
#define CFG_LIGHT_RELAY_PIN   DIO_PIN5

/* ---- Fan motor (H-bridge): direction pins + PWM on Timer0 (ENABLE=OC0A=PB7). */
#define CFG_FAN_IN1_PORT      DIO_PORTA
#define CFG_FAN_IN1_PIN       DIO_PIN6
#define CFG_FAN_IN2_PORT      DIO_PORTA
#define CFG_FAN_IN2_PIN       DIO_PIN7
#define CFG_FAN_PWM_TIMER     Timer0            /* ENABLE pin must be wired to OC0A = PB7 */

/* ---- Buzzer: ACTIVE type on PB3, active-high (Timer-free, leaves timers for the fan). */
#define CFG_BUZZER_PORT       DIO_PORTB
#define CFG_BUZZER_PIN        DIO_PIN5

/* ---- Status LED (system alive / alarm indicator). */
#define CFG_STATUS_LED_PORT   DIO_PORTB
#define CFG_STATUS_LED_PIN    DIO_PIN4

/* ---- Door sensor (reed switch) on an external-interrupt pin. */
#define CFG_DOOR_EXT_LINE     EXT_INT0          /* INT0 = PD0 */
#define CFG_DOOR_PORT         DIO_PORTD
#define CFG_DOOR_PIN          DIO_PIN0

/* ---- "Forgot password" recovery button on another external interrupt. */
#define CFG_RECOVERY_EXT_LINE EXT_INT1          /* INT1 = PD1 */
#define CFG_RECOVERY_PORT     DIO_PORTD
#define CFG_RECOVERY_PIN      DIO_PIN1

/* ---- SD card chip-select (SCK/MOSI/MISO are owned by the SPI driver). */
#define CFG_SD_CS_PORT        DIO_PORTB
#define CFG_SD_CS_PIN         DIO_PIN0

/*========================================================================
 * 4. CONTROL THRESHOLDS (AUTO MODE)
 *======================================================================*/
/* Light: turn the lamp ON when the room is darker than this percentage. */
#define CFG_LIGHT_ON_PERCENT  30                /* below 30% brightness -> lamp ON */

/* Fan temperature thresholds (degrees Celsius). */
#define CFG_TEMP_FAN_LOW      25                /* below this: fan OFF */
#define CFG_TEMP_FAN_HIGH     35                /* 25..35: 50% ; above 35: 100% */
#define CFG_TEMP_ALARM        45                /* above this: over-temp alarm + buzzer */

/* Fan speed percentages for the bands above. */
#define CFG_FAN_SPEED_MED     50                /* 25..35 C */
#define CFG_FAN_SPEED_HIGH    100               /* >35 C */

/*========================================================================
 * 5. SECURITY SETTINGS
 *======================================================================*/
#define CFG_PASSWORD_LEN      4                 /* 4-digit password */
#define CFG_MAX_ATTEMPTS      3                 /* wrong tries before intruder lockout */
#define CFG_LOCKOUT_SECONDS   60                /* lockout countdown length (seconds) */

/* Default factory password (used the first time, before the user changes it). */
#define CFG_DEFAULT_PW0       '1'
#define CFG_DEFAULT_PW1       '2'
#define CFG_DEFAULT_PW2       '3'
#define CFG_DEFAULT_PW3       '4'

/*========================================================================
 * 6. TIMING / TASK PERIODS (milliseconds)
 *======================================================================*/
#define CFG_PERIOD_SENSOR_MS    1000            /* read sensors once per second */
#define CFG_PERIOD_KEYPAD_MS    20              /* scan keypad every 20 ms (debounce) */
#define CFG_PERIOD_DISPLAY_MS   500             /* refresh the LCD twice per second */
#define CFG_PERIOD_CONTROL_MS   200             /* run the control logic 5x per second */
#define CFG_PERIOD_HEARTBEAT_MS 250             /* blink LED + kick watchdog */

/*========================================================================
 * 7. EEPROM MEMORY MAP (byte addresses for persistent settings)
 *======================================================================*/
#define CFG_EE_MAGIC_ADDR      0                /* 1 byte : "initialised" marker */
#define CFG_EE_MAGIC_VALUE     0xA5             /* value that proves EEPROM was set up */
#define CFG_EE_PASSWORD_ADDR   1                /* 4 bytes: the saved password */
#define CFG_EE_LASTMODE_ADDR   8                /* 1 byte : last system mode */
#define CFG_EE_LOCKOUT_ADDR    9                /* 1 byte : intruder-lockout flag (survives reset) */
#define CFG_EE_ANSWER_ADDR     16               /* up to 8 bytes: recovery answer */

#endif /* SYSTEM_CONFIG_H_ */
