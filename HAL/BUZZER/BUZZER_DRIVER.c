/*
 * BUZZER_DRIVER.c
 *
 * Layer  : HAL
 * Target : ATmega2560 @ 16MHz
 * Device : active or passive buzzer.
 *
 * Passive-tone method:
 *   We run the chosen timer in CTC mode and let it TOGGLE its OC pin on each
 *   compare match. Toggling on every match produces a square wave whose
 *   frequency is:  f_out = F_CPU / (2 * prescaler * (OCR + 1)).
 *   So for a target frequency we solve for OCR:
 *       OCR = F_CPU / (2 * prescaler * f_out) - 1
 *   We use prescaler = 64, which covers the audible range with an 8-bit OCR.
 *
 * Author : (Mohamed Eid)
 */

#include "BUZZER_DRIVER.h"   /* Public types, enums and prototypes */
#include <util/delay.h>      /* _delay_ms for the blocking BUZZER_beep helper */
#include <avr/io.h>          /* Direct OC-pin / COM-bit access for the toggle mode */

/*------------------------------------------------------------------------
 * Saved configuration.
 *----------------------------------------------------------------------*/
static BUZZER_Config g_bz;     /* Private copy of the active configuration */

/* Prescaler used for the passive tone maths (must match what we program). */
#define BZ_PRESCALER   64UL    /* /64 keeps OCR within 8 bits for audible tones */
#define BZ_DEFAULT_HZ  2500    /* Default passive tone if none set */

/*------------------------------------------------------------------------
 * DEFAULT configuration used when BUZZER_init() receives NULL.
 * Active buzzer on PB1, active-high (matches the earlier alarm wiring).
 *----------------------------------------------------------------------*/
const BUZZER_Config BUZZER_DEFAULT_CONFIG = {
	.m_type     = BUZZER_ACTIVE,        /* Default is an active buzzer */
	.m_port     = DIO_PORTB,            /* On PORTB ... */
	.m_pin      = DIO_PIN1,             /* ... pin 1 */
	.m_polarity = BUZZER_ACTIVE_HIGH,   /* HIGH = sound */
	.m_timer    = Timer2                /* (only used if type is passive) */
};

/*------------------------------------------------------------------------
 * Helper: enable the OC-pin TOGGLE-on-compare bits for the chosen timer,
 * and make that OC pin an output. Toggling gives us the square wave.
 *----------------------------------------------------------------------*/
static void bz_enable_toggle_output(void)
{
	if(g_bz.m_timer == Timer0)
	{
		TCCR0A |= (1 << COM0A0);          /* COM0A1:0 = 01 -> toggle OC0A on match */
		TCCR0A &= ~(1 << COM0A1);
		DIO_setDirection(DIO_PORTB, DIO_PIN7, DIO_OUTPUT);   /* OC0A = PB7 */
	}
	else /* Timer2 */
	{
		TCCR2A |= (1 << COM2A0);          /* COM2A1:0 = 01 -> toggle OC2A on match */
		TCCR2A &= ~(1 << COM2A1);
		DIO_setDirection(DIO_PORTB, DIO_PIN4, DIO_OUTPUT);   /* OC2A = PB4 */
	}
}

/*------------------------------------------------------------------------
 * Helper: disconnect the OC pin from the timer so the buzzer goes silent.
 *----------------------------------------------------------------------*/
static void bz_disable_toggle_output(void)
{
	if(g_bz.m_timer == Timer0)
	{
		TCCR0A &= ~((1 << COM0A0) | (1 << COM0A1));   /* Normal port mode -> OC0A released */
		DIO_writePin(DIO_PORTB, DIO_PIN7, DIO_LOW);   /* Park the pin LOW (silent) */
	}
	else
	{
		TCCR2A &= ~((1 << COM2A0) | (1 << COM2A1));   /* Normal port mode -> OC2A released */
		DIO_writePin(DIO_PORTB, DIO_PIN4, DIO_LOW);   /* Park the pin LOW */
	}
}

/*========================================================================
 * BUZZER_init : set up the pin (active) or the timer (passive).
 *======================================================================*/
void BUZZER_init(const BUZZER_Config* config)
{
	if(config == 0)              /* NULL-safe: use the default config */
	{
		config = &BUZZER_DEFAULT_CONFIG;
	}
	g_bz = *config;              /* Keep a private copy of the configuration */

	if(g_bz.m_type == BUZZER_ACTIVE)
	{
		DIO_setDirection(g_bz.m_port, g_bz.m_pin, DIO_OUTPUT);   /* Buzzer pin = output */
		BUZZER_off();                                            /* Start silent */
	}
	else /* BUZZER_PASSIVE */
	{
		/* Configure the timer in CTC mode with a default tone, but keep the
		 * OC pin disconnected until BUZZER_on() so it starts silent. */
		TIMER_CONFIG_SELCECTION tcfg;
		tcfg.m_timer_id        = g_bz.m_timer;            /* Chosen timer */
		tcfg.m_timer_mode      = TIMER_MODE_CTC;          /* CTC so we can pick the period */
		tcfg.m_timer_prescaler = TIMER_PRESCALER_64;      /* Matches BZ_PRESCALER */
		tcfg.m_ocr_value       = 124;                     /* Temporary; set by BUZZER_setTone */
		tcfg.callback          = 0;                       /* No ISR needed; the pin toggles itself */
		TIMER_setup(&tcfg);                               /* Start the timer */

		bz_disable_toggle_output();                       /* Silent until BUZZER_on() */
		BUZZER_setTone(BZ_DEFAULT_HZ);                    /* Pre-load the default frequency */
	}
}

/*========================================================================
 * BUZZER_setTone : passive mode only - choose the tone frequency.
 *======================================================================*/
void BUZZER_setTone(uint16_t freq_hz)
{
	if(g_bz.m_type != BUZZER_PASSIVE) return;   /* Active buzzers have a fixed pitch */
	if(freq_hz == 0) return;                    /* Guard against divide-by-zero */

	/* OCR = F_CPU / (2 * prescaler * f) - 1.  Clamp to the 8-bit OCR range. */
	uint32_t ocr = (F_CPU / (2UL * BZ_PRESCALER * (uint32_t)freq_hz));
	if(ocr > 0) ocr -= 1;                       /* Apply the "-1" of the formula */
	if(ocr > 255) ocr = 255;                    /* Timer0/2 OCR is 8-bit, so clamp */

	TIMER_setOCR(g_bz.m_timer, (uint8_t)ocr);   /* Program the new period -> new pitch */
}

/*========================================================================
 * BUZZER_on : start sounding.
 *======================================================================*/
void BUZZER_on(void)
{
	if(g_bz.m_type == BUZZER_ACTIVE)
	{
		/* Active: just drive the pin to its "sound" level (polarity aware). */
		if(g_bz.m_polarity == BUZZER_ACTIVE_HIGH)
			DIO_writePin(g_bz.m_port, g_bz.m_pin, DIO_HIGH);   /* HIGH = sound */
		else
			DIO_writePin(g_bz.m_port, g_bz.m_pin, DIO_LOW);    /* LOW = sound */
	}
	else
	{
		bz_enable_toggle_output();   /* Passive: connect the OC pin so the square wave reaches it */
	}
}

/*========================================================================
 * BUZZER_off : stop sounding.
 *======================================================================*/
void BUZZER_off(void)
{
	if(g_bz.m_type == BUZZER_ACTIVE)
	{
		/* Active: drive the pin to its "silent" level (polarity aware). */
		if(g_bz.m_polarity == BUZZER_ACTIVE_HIGH)
			DIO_writePin(g_bz.m_port, g_bz.m_pin, DIO_LOW);    /* LOW = silent */
		else
			DIO_writePin(g_bz.m_port, g_bz.m_pin, DIO_HIGH);   /* HIGH = silent */
	}
	else
	{
		bz_disable_toggle_output();   /* Passive: disconnect the OC pin -> silence */
	}
}

/*========================================================================
 * BUZZER_beep : sound for 'ms' milliseconds then stop (blocking helper).
 * NOTE: blocking. Fine for a quick UI beep; from a task prefer on()/off()
 * with a vTaskDelay in between so other tasks keep running.
 *======================================================================*/
void BUZZER_beep(uint16_t ms)
{
	BUZZER_on();                 /* Start the sound */
	while(ms--)                  /* Wait the requested number of milliseconds ... */
	{
		_delay_ms(1);            /* ... one millisecond at a time */
	}
	BUZZER_off();                /* Stop the sound */
}
