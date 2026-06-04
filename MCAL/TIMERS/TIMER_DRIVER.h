/*
 * TIMER_DRIVER.h
 *
 * Layer  : MCAL
 * Target : ATmega2560 @ 16MHz
 * Purpose: General-purpose timer driver (Normal / CTC / Fast-PWM), adapted
 *          from the author's ATmega32 code.
 *
 * IMPORTANT reservation:
 *   - Timer1 is OWNED by FreeRTOS (system tick) -> do NOT use it here.
 *   - Timer5 is OWNED by the ICU driver        -> do NOT use it here.
 *   This driver therefore exposes the safe 8-bit timers Timer0 and Timer2,
 *   which is plenty for buzzer tones, soft delays, and periodic callbacks.
 *
 * 2560 vs ATmega32 differences handled here:
 *   - The interrupt-mask register is split per timer: TIMSK0, TIMSK2.
 *   - OC0A pin = PB7, OC2A pin = PB4 on the 2560 (used only in PWM mode).
 *
 * Author : (Mohamed Eid)
 */

#ifndef TIMER_DRIVER_H_        /* Include guard opening */
#define TIMER_DRIVER_H_        /* Guard token */

#include <avr/io.h>            /* AVR register map (TCCR0A/B, TCCR2A/B, TIMSK0/2 ...) */
#include <avr/interrupt.h>     /* ISR() macro */
#include <stdint.h>            /* Fixed-width integer types */

/*------------ MACROS : single-bit helpers ------------------------------*/
#define SET_BIT(reg,bit)    ((reg)|=(1<<(bit)))    /* Set one bit HIGH */
#define CLEAR_BIT(reg,bit)  ((reg)&=~(1<<(bit)))   /* Clear one bit LOW */
#define TOGGLE_BIT(reg,bit) ((reg)^=(1<<(bit)))    /* Flip one bit */
#define CHECK_BIT(reg,bit)  ((reg)&(1<<(bit)))     /* Test one bit */

/*------------------------------------------------------------------------
 * ENUM : which timer to use. Only the RTOS/ICU-safe timers are exposed.
 *----------------------------------------------------------------------*/
typedef enum {
	Timer0 = 0,   /* 8-bit, OC0A = PB7 */
	Timer2        /* 8-bit, OC2A = PB4 */
}TIMER_SELECT_ID;

/*------------------------------------------------------------------------
 * ENUM : clock prescaler. (Timer2 additionally supports 32 and 128.)
 *----------------------------------------------------------------------*/
typedef enum {
	TIMER_PRESCALER_1    = 1,
	TIMER_PRESCALER_8    = 8,
	TIMER_PRESCALER_32   = 32,    /* Timer2 only */
	TIMER_PRESCALER_64   = 64,
	TIMER_PRESCALER_128  = 128,   /* Timer2 only */
	TIMER_PRESCALER_256  = 256,
	TIMER_PRESCALER_1024 = 1024
}TIMER_PRESCALER_SELECT;

/*------------------------------------------------------------------------
 * ENUM : operating mode.
 *----------------------------------------------------------------------*/
typedef enum {
	TIMER_MODE_NORMAL,     /* Free-running, overflow interrupt */
	TIMER_MODE_CTC,        /* Clear-on-compare, compare-match interrupt */
	TIMER_MODE_FAST_PWM    /* Fast PWM on the OC pin (no interrupt) */
}TIMER_MODE_SELECT;

/*------------------------------------------------------------------------
 * STRUCT : full timer configuration.
 *----------------------------------------------------------------------*/
typedef struct {
	TIMER_SELECT_ID        m_timer_id;          /* Timer0 or Timer2 */
	TIMER_MODE_SELECT      m_timer_mode;        /* Normal / CTC / Fast-PWM */
	TIMER_PRESCALER_SELECT m_timer_prescaler;   /* Clock divider */
	uint8_t                m_ocr_value;         /* Compare value (CTC top, or PWM duty 0..255) */
	void (*callback)(void);                     /* ISR callback for Normal/CTC (NULL = none) */
}TIMER_CONFIG_SELCECTION;

/*================================ APIs ==================================*/

/* Configure and start a timer. Pass NULL to use TIMER_DEFAULT_CONFIG. */
void TIMER_setup(const TIMER_CONFIG_SELCECTION* config);

/* Start a (re)configured timer by applying a prescaler / clock source. */
void TIMER_start(TIMER_SELECT_ID id, TIMER_PRESCALER_SELECT pre);

/* Stop a timer (clock source removed). */
void TIMER_stop(TIMER_SELECT_ID id);

/* Reset a timer's counter to zero. */
void TIMER_reset(TIMER_SELECT_ID id);

/* Update the compare value at run time (e.g. change PWM duty). */
void TIMER_setOCR(TIMER_SELECT_ID id, uint8_t value);

/* Ready-made default config (Timer0, CTC, /64, OCR=249, no callback). */
extern const TIMER_CONFIG_SELCECTION TIMER_DEFAULT_CONFIG;

#endif /* TIMER_DRIVER_H_ */
