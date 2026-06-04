/*
 * TIMER_DRIVER.c
 *
 * Layer  : MCAL
 * Target : ATmega2560 @ 16MHz
 * Purpose: Timer driver implementation for the two RTOS/ICU-safe 8-bit
 *          timers (Timer0 and Timer2), adapted from the author's code.
 *
 * On the 2560 each timer has its own control register pair (TCCRnA/TCCRnB)
 * and its own interrupt-mask register (TIMSK0 / TIMSK2). Bit names for the
 * mode/prescaler are per-timer; we use them explicitly per case.
 *
 * Author : (Mohamed Eid)
 */

#include "TIMER_DRIVER.h"     /* Public types, enums and prototypes */

static void (*volatile timer0_cb)(void) = 0;   /* Timer0 ISR callback */
static void (*volatile timer2_cb)(void) = 0;   /* Timer2 ISR callback */

/*------------------------------------------------------------------------
 * DEFAULT configuration used when TIMER_setup() receives NULL.
 * Timer0, CTC, /64, OCR=249  ->  at 16MHz that is a 1 kHz (1 ms) tick if a
 * callback is attached. Safe, generally useful default.
 *----------------------------------------------------------------------*/
const TIMER_CONFIG_SELCECTION TIMER_DEFAULT_CONFIG = {
	.m_timer_id        = Timer0,              /* Default timer */
	.m_timer_mode      = TIMER_MODE_CTC,      /* Default CTC mode */
	.m_timer_prescaler = TIMER_PRESCALER_64,  /* Default /64 */
	.m_ocr_value       = 249,                 /* (249+1)*64/16MHz = 1 ms */
	.callback          = 0                    /* No callback by default */
};

/*------------------------------------------------------------------------
 * Helper: program Timer0 prescaler bits (CS02:CS00 in TCCR0B).
 *----------------------------------------------------------------------*/
static void _t0_prescaler(TIMER_PRESCALER_SELECT pre)
{
	TCCR0B &= ~0x07;                              /* Clear the 3 clock-select bits first */
	switch(pre) {
		case TIMER_PRESCALER_1:    TCCR0B |= (1<<CS00); break;                 /* /1 */
		case TIMER_PRESCALER_8:    TCCR0B |= (1<<CS01); break;                 /* /8 */
		case TIMER_PRESCALER_64:   TCCR0B |= (1<<CS01)|(1<<CS00); break;       /* /64 */
		case TIMER_PRESCALER_256:  TCCR0B |= (1<<CS02); break;                 /* /256 */
		case TIMER_PRESCALER_1024: TCCR0B |= (1<<CS02)|(1<<CS00); break;       /* /1024 */
		default:                   TCCR0B |= (1<<CS01)|(1<<CS00); break;       /* Fallback /64 */
	}
}

/*------------------------------------------------------------------------
 * Helper: program Timer2 prescaler bits (CS22:CS20 in TCCR2B).
 * Timer2 has the extra /32 and /128 options.
 *----------------------------------------------------------------------*/
static void _t2_prescaler(TIMER_PRESCALER_SELECT pre)
{
	TCCR2B &= ~0x07;                              /* Clear the 3 clock-select bits first */
	switch(pre) {
		case TIMER_PRESCALER_1:    TCCR2B |= (1<<CS20); break;                 /* /1 */
		case TIMER_PRESCALER_8:    TCCR2B |= (1<<CS21); break;                 /* /8 */
		case TIMER_PRESCALER_32:   TCCR2B |= (1<<CS21)|(1<<CS20); break;       /* /32 */
		case TIMER_PRESCALER_64:   TCCR2B |= (1<<CS22); break;                 /* /64 */
		case TIMER_PRESCALER_128:  TCCR2B |= (1<<CS22)|(1<<CS20); break;       /* /128 */
		case TIMER_PRESCALER_256:  TCCR2B |= (1<<CS22)|(1<<CS21); break;       /* /256 */
		case TIMER_PRESCALER_1024: TCCR2B |= (1<<CS22)|(1<<CS21)|(1<<CS20); break; /* /1024 */
		default:                   TCCR2B |= (1<<CS22); break;                 /* Fallback /64 */
	}
}

/*========================================================================
 * TIMER_setup : configure and start one timer.
 *======================================================================*/
void TIMER_setup(const TIMER_CONFIG_SELCECTION* config)
{
	if(config == 0)                  /* NULL-safe: use the default config */
	{
		config = &TIMER_DEFAULT_CONFIG;
	}

	if(config->m_timer_id == Timer0)             /* ---------- Timer0 ---------- */
	{
		timer0_cb = config->callback;            /* Remember the callback */
		TCCR0A = 0; TCCR0B = 0; TCNT0 = 0;       /* Reset control + counter */
		TIMSK0 = 0;                              /* Clear Timer0 interrupt masks */

		switch(config->m_timer_mode)
		{
			case TIMER_MODE_NORMAL:                              /* Overflow mode */
				if(config->callback) SET_BIT(TIMSK0, TOIE0);     /* Enable overflow interrupt if a callback exists */
				break;
			case TIMER_MODE_CTC:                                 /* Clear-Timer-on-Compare */
				SET_BIT(TCCR0A, WGM01);                          /* WGM01=1 selects CTC */
				OCR0A = config->m_ocr_value;                     /* Compare top value */
				if(config->callback) SET_BIT(TIMSK0, OCIE0A);    /* Enable compare-match interrupt */
				break;
			case TIMER_MODE_FAST_PWM:                            /* Fast PWM on OC0A (PB7) */
				SET_BIT(TCCR0A, WGM00); SET_BIT(TCCR0A, WGM01);  /* WGM01:00 = 11 -> Fast PWM */
				SET_BIT(TCCR0A, COM0A1);                         /* Non-inverting output on OC0A */
				OCR0A = config->m_ocr_value;                     /* Duty = OCR0A/255 */
				SET_BIT(DDRB, PB7);                              /* OC0A pin is PB7 -> make it output */
				break;
		}
		_t0_prescaler(config->m_timer_prescaler);                /* Apply prescaler (also starts the clock) */
	}
	else                                          /* ---------- Timer2 ---------- */
	{
		timer2_cb = config->callback;            /* Remember the callback */
		TCCR2A = 0; TCCR2B = 0; TCNT2 = 0;       /* Reset control + counter */
		TIMSK2 = 0;                              /* Clear Timer2 interrupt masks */

		switch(config->m_timer_mode)
		{
			case TIMER_MODE_NORMAL:
				if(config->callback) SET_BIT(TIMSK2, TOIE2);     /* Overflow interrupt */
				break;
			case TIMER_MODE_CTC:
				SET_BIT(TCCR2A, WGM21);                          /* CTC mode */
				OCR2A = config->m_ocr_value;                     /* Compare top */
				if(config->callback) SET_BIT(TIMSK2, OCIE2A);    /* Compare-match interrupt */
				break;
			case TIMER_MODE_FAST_PWM:                            /* Fast PWM on OC2A (PB4) */
				SET_BIT(TCCR2A, WGM20); SET_BIT(TCCR2A, WGM21);  /* Fast PWM */
				SET_BIT(TCCR2A, COM2A1);                         /* Non-inverting on OC2A */
				OCR2A = config->m_ocr_value;                     /* Duty = OCR2A/255 */
				SET_BIT(DDRB, PB4);                              /* OC2A pin is PB4 -> output */
				break;
		}
		_t2_prescaler(config->m_timer_prescaler);                /* Apply prescaler (starts the clock) */
	}

	if(config->callback != 0)        /* If any interrupt mode was chosen ... */
	{
		sei();                       /* ... ensure global interrupts are enabled */
	}
}

/*========================================================================
 * TIMER_start / TIMER_stop / TIMER_reset / TIMER_setOCR : runtime control.
 *======================================================================*/
void TIMER_start(TIMER_SELECT_ID id, TIMER_PRESCALER_SELECT pre)
{
	if(id == Timer0) _t0_prescaler(pre);     /* Re-apply a clock source to Timer0 */
	else             _t2_prescaler(pre);     /* Or to Timer2 */
}

void TIMER_stop(TIMER_SELECT_ID id)
{
	if(id == Timer0) TCCR0B &= ~0x07;        /* Remove Timer0 clock (CS bits = 0) */
	else             TCCR2B &= ~0x07;        /* Remove Timer2 clock */
}

void TIMER_reset(TIMER_SELECT_ID id)
{
	if(id == Timer0) TCNT0 = 0;              /* Zero the Timer0 counter */
	else             TCNT2 = 0;              /* Zero the Timer2 counter */
}

void TIMER_setOCR(TIMER_SELECT_ID id, uint8_t value)
{
	if(id == Timer0) OCR0A = value;          /* Update Timer0 compare value */
	else             OCR2A = value;          /* Update Timer2 compare value */
}

/*========================================================================
 * ISRs : forward each timer event to its callback (if any).
 *======================================================================*/
ISR(TIMER0_OVF_vect)  { if(timer0_cb) timer0_cb(); }   /* Timer0 overflow */
ISR(TIMER0_COMPA_vect){ if(timer0_cb) timer0_cb(); }   /* Timer0 compare match */
ISR(TIMER2_OVF_vect)  { if(timer2_cb) timer2_cb(); }   /* Timer2 overflow */
ISR(TIMER2_COMPA_vect){ if(timer2_cb) timer2_cb(); }   /* Timer2 compare match */
