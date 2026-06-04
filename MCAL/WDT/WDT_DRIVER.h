/*
 * WDT_DRIVER.h
 *
 * Layer   : MCAL (Microcontroller Abstraction Layer)
 * Target  : ATmega2560 @ 16MHz
 * Purpose : Watchdog Timer driver. The watchdog is a free-running counter
 *           driven by an independent 128kHz on-chip oscillator. If the
 *           application does not "kick" (reset) it within the chosen
 *           timeout, the watchdog resets the whole MCU. This rescues the
 *           system from lock-ups / infinite loops and is a hallmark of
 *           reliable embedded firmware.
 *
 * Modes   : - SYSTEM RESET  : timeout -> hardware reset (the classic use).
 *           - INTERRUPT     : timeout -> fire WDT_vect once (e.g. to save
 *                             state or wake from sleep).
 *           - INTERRUPT+RESET: first timeout = interrupt, second = reset.
 *
 * Usage   : 1) Fill a WDT_CONFIG (or pass NULL for the default).
 *           2) Call WDT_init(&cfg).
 *           3) Call WDT_reset() periodically from a healthy code path.
 *
 * Author  : (Mohamed Eid)
 */

#ifndef WDT_DRIVER_H_          /* Include guard opening */
#define WDT_DRIVER_H_          /* Guard token */

#include <avr/io.h>            /* AVR register map (WDTCSR, MCUSR ...) */
#include <avr/interrupt.h>     /* ISR() macro for the watchdog interrupt mode */
#include <stdint.h>            /* Fixed-width integer types */

/*------------ MACROS : single-bit helpers ------------------------------*/
#define SET_BIT(reg,bit)    ((reg)|=(1<<(bit)))    /* Set one bit HIGH */
#define CLEAR_BIT(reg,bit)  ((reg)&=~(1<<(bit)))   /* Clear one bit LOW */
#define CHECK_BIT(reg,bit)  ((reg)&(1<<(bit)))     /* Test one bit */

/*------------------------------------------------------------------------
 * ENUM : timeout period. Codes are the WDP3:WDP0 prescaler values from the
 * datasheet. Times are approximate at 5V because the 128kHz RC oscillator
 * drifts a little with voltage/temperature.
 *----------------------------------------------------------------------*/
typedef enum {
	WDT_TIMEOUT_16MS   = 0x00,   /* ~16   ms */
	WDT_TIMEOUT_32MS   = 0x01,   /* ~32   ms */
	WDT_TIMEOUT_64MS   = 0x02,   /* ~64   ms */
	WDT_TIMEOUT_125MS  = 0x03,   /* ~0.125 s */
	WDT_TIMEOUT_250MS  = 0x04,   /* ~0.25  s */
	WDT_TIMEOUT_500MS  = 0x05,   /* ~0.5   s */
	WDT_TIMEOUT_1S     = 0x06,   /* ~1     s */
	WDT_TIMEOUT_2S     = 0x07,   /* ~2     s */
	WDT_TIMEOUT_4S     = 0x20,   /* ~4     s (WDP3 set, hence bit5 = 0x20) */
	WDT_TIMEOUT_8S     = 0x21    /* ~8     s */
}WDT_TIMEOUT;

/*------------------------------------------------------------------------
 * ENUM : what the watchdog does when the timeout elapses.
 *----------------------------------------------------------------------*/
typedef enum {
	WDT_MODE_RESET = 0,        /* Timeout performs a hardware system reset */
	WDT_MODE_INTERRUPT,        /* Timeout fires WDT_vect (no reset) */
	WDT_MODE_INTERRUPT_RESET   /* First timeout = interrupt, next = reset */
}WDT_MODE;

/*------------------------------------------------------------------------
 * STRUCT : full watchdog configuration.
 *----------------------------------------------------------------------*/
typedef struct {
	WDT_TIMEOUT m_timeout;     /* How long before the watchdog bites */
	WDT_MODE    m_mode;        /* Reset / interrupt / interrupt-then-reset */
	void (*callback)(void);    /* Handler for the interrupt modes (NULL = none) */
}WDT_CONFIG;

/*================================ APIs ==================================*/

/* Start the watchdog with the given configuration. Pass NULL for DEFAULT_CONFIG. */
void WDT_init(const WDT_CONFIG* config);

/* "Kick" the watchdog: restart its counter. Call this regularly while healthy. */
void WDT_reset(void);

/* Stop the watchdog completely (e.g. before a long blocking operation). */
void WDT_stop(void);

/* Read and clear the reset-source flags captured at boot in MCUSR.
 * Returns the raw MCUSR snapshot so the app can tell WHY it last reset
 * (power-on, external, brown-out, or watchdog). Call once early in main(). */
uint8_t WDT_getResetSource(void);

#endif /* WDT_DRIVER_H_ */
