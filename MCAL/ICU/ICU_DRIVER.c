/*
 * ICU_DRIVER.c
 *
 * Layer  : MCAL
 * Target : ATmega2560 @ 16MHz, Timer5, capture pin ICP5 = PL1
 * Purpose: Implementation of the Input Capture Unit driver.
 *
 * How it works:
 *   Timer5 free-runs in Normal mode. Each selected edge on ICP5 copies
 *   TCNT5 into ICR5 and fires TIMER5_CAPT_vect. We time-stamp the first
 *   edge, then on the second edge subtract to get the elapsed ticks, while
 *   counting Timer5 overflows so we can measure pulses longer than 65535
 *   ticks. Ticks are converted to microseconds using the prescaler.
 *
 * Author : (Mohamed Eid)
 */

#include "ICU_DRIVER.h"        /* Public types, enums and prototypes */

/*------------------------------------------------------------------------
 * Internal state shared between the API and the ISRs. 'volatile' because
 * the ISRs modify these asynchronously to the main program.
 *----------------------------------------------------------------------*/
static volatile uint16_t _t1      = 0;    /* Timer value captured on the first edge */
static volatile uint8_t  _state   = 0;    /* 0 = waiting for first edge, 1 = waiting for second */
static volatile uint16_t _ovf_cnt = 0;    /* Number of Timer5 overflows since the first edge */

static ICU_Mode  _mode    = ICU_MODE_PULSE_WIDTH;   /* Active measurement mode */
static uint32_t  _tick_ns = 1000;                   /* Duration of one timer tick, in nanoseconds */
static void (*_on_time)(uint32_t) = 0;              /* Callback for pulse-width / period results */
static void (*_on_freq)(uint32_t) = 0;              /* Callback for frequency results */

/*========================= helper functions ============================*/

/* Translate a prescaler enum into the CS52:CS50 bits for TCCR5B. */
static uint8_t _prescaler_bits(ICU_Prescaler pre)
{
	switch (pre) {
		case ICU_PRESCALER_1:    return (1 << CS50);                 /* clk / 1 */
		case ICU_PRESCALER_8:    return (1 << CS51);                 /* clk / 8 */
		case ICU_PRESCALER_64:   return (1 << CS51) | (1 << CS50);   /* clk / 64 */
		case ICU_PRESCALER_256:  return (1 << CS52);                 /* clk / 256 */
		case ICU_PRESCALER_1024: return (1 << CS52) | (1 << CS50);   /* clk / 1024 */
		default:                 return (1 << CS51);                 /* Safe fallback: clk / 8 */
	}
}

/* Pick a prescaler automatically so the tick is sub-microsecond at 16 MHz. */
static ICU_Prescaler _auto_prescaler(void)
{
	uint32_t mhz = F_CPU / 1000000UL;      /* CPU speed in MHz */
	if (mhz <= 1)   return ICU_PRESCALER_1;     /* Very slow clock: no division */
	if (mhz <= 8)   return ICU_PRESCALER_8;     /* Up to 8 MHz: divide by 8 */
	if (mhz <= 64)  return ICU_PRESCALER_64;    /* Up to 64 MHz: divide by 64 */
	if (mhz <= 256) return ICU_PRESCALER_256;   /* Up to 256 MHz: divide by 256 */
	return ICU_PRESCALER_1024;                  /* Anything faster: divide by 1024 */
}

/* Compute the duration of one tick in nanoseconds for the chosen prescaler. */
static uint32_t _calc_tick_ns(ICU_Prescaler pre)
{
	uint32_t pval;                          /* Numeric prescaler value */
	switch (pre) {
		case ICU_PRESCALER_1:    pval = 1;    break;
		case ICU_PRESCALER_8:    pval = 8;    break;
		case ICU_PRESCALER_64:   pval = 64;   break;
		case ICU_PRESCALER_256:  pval = 256;  break;
		case ICU_PRESCALER_1024: pval = 1024; break;
		default:                 pval = 8;    break;
	}
	/* tick_ns = (prescaler / F_CPU) * 1e9 = prescaler * 1000 / F_CPU_MHz */
	return (pval * 1000UL) / (F_CPU / 1000000UL);
}

/*------------------------------------------------------------------------
 * DEFAULT configuration (exported). Pulse-width mode, AUTO prescaler,
 * noise canceler ON, no callbacks attached yet.
 *----------------------------------------------------------------------*/
const ICU_Config ICU_DEFAULT_CONFIG = {
	.mode         = ICU_MODE_PULSE_WIDTH,   /* Default measures pulse width */
	.prescaler    = ICU_PRESCALER_AUTO,     /* Let the driver choose the prescaler */
	.noise_cancel = ICU_NC_ON,              /* Filter glitches by default */
	.on_time_us   = 0,                      /* No time callback yet */
	.on_freq_hz   = 0                       /* No frequency callback yet */
};

/*================================ APIs ==================================*/

void ICU_init(const ICU_Config *cfg)
{
	if (cfg == 0)                       /* NULL-safe: if no config supplied ... */
	{
		cfg = &ICU_DEFAULT_CONFIG;      /* ... fall back to the exported default */
	}

	/* Save the chosen mode and callbacks into the internal state. */
	_mode    = cfg->mode;               /* Remember what we are measuring */
	_on_time = cfg->on_time_us;         /* Time-result callback (pulse/period) */
	_on_freq = cfg->on_freq_hz;         /* Frequency-result callback */
	_state   = 0;                       /* Start by waiting for the first edge */
	_ovf_cnt = 0;                       /* No overflows yet */

	/* Resolve AUTO to a concrete prescaler and pre-compute the tick time. */
	ICU_Prescaler pre = (cfg->prescaler == ICU_PRESCALER_AUTO)
	                    ? _auto_prescaler()     /* Auto-select if requested */
	                    : cfg->prescaler;       /* Otherwise honour the user's choice */
	_tick_ns = _calc_tick_ns(pre);      /* Cache the nanoseconds-per-tick value */

	/* Make the capture pin ICP5 (PL1) an input so edges can be detected. */
	CLEAR_BIT(DDRL, PL1);               /* DDRL bit 1 = 0 -> PL1 is an input */

	/* Timer5 Normal mode: no waveform generation. */
	TCCR5A = 0;                         /* Clear WGM low bits / compare-output bits */

	/* Build TCCR5B: noise canceler + first-edge polarity + prescaler. */
	TCCR5B = 0;                                         /* Start from a clean control register */
	if (cfg->noise_cancel == ICU_NC_ON)                 /* If glitch filtering was requested ... */
		TCCR5B |= (1 << ICNC5);                         /* ... enable the 4-sample input noise canceler */
	TCCR5B |= (1 << ICES5);                             /* Capture on the RISING edge first */
	TCCR5B |= _prescaler_bits(pre);                     /* Apply the prescaler (also starts the clock) */

	TCNT5 = 0;                          /* Reset the timer counter to a known start */

	/* Enable Timer5 Input Capture interrupt + Overflow interrupt. */
	TIMSK5 |= (1 << ICIE5) | (1 << TOIE5);   /* ICIE5 = capture int, TOIE5 = overflow int */

	sei();                              /* Global interrupt enable so the ISRs can run */
}

void ICU_stop(void)
{
	TCCR5B = 0;                                  /* Remove the clock source -> Timer5 stops */
	TCCR5A = 0;                                  /* Clear the other control bits */
	TIMSK5 &= ~((1 << ICIE5) | (1 << TOIE5));    /* Disable the capture and overflow interrupts */
	_on_time = 0;                                /* Drop the callbacks so stale results are not fired */
	_on_freq = 0;
}

/*========================================================================
 * ISR - Timer5 Overflow: extends the 16-bit range by counting wraps.
 *======================================================================*/
ISR(TIMER5_OVF_vect)
{
	_ovf_cnt++;                          /* One more full 65536-tick wrap has occurred */
}

/*========================================================================
 * ISR - Timer5 Input Capture: the core measurement logic.
 *======================================================================*/
ISR(TIMER5_CAPT_vect)
{
	uint16_t now = ICR5;                 /* Read the captured timer value (the edge time-stamp) */

	if (_mode == ICU_MODE_PULSE_WIDTH)   /* ---- measuring a single pulse width ---- */
	{
		if (_state == 0)                 /* First (rising) edge: start of the pulse */
		{
			_t1      = now;              /* Remember the start time-stamp */
			_ovf_cnt = 0;                /* Reset the overflow counter for this measurement */
			CLEAR_BIT(TCCR5B, ICES5);    /* Switch to capture the FALLING edge next */
			_state   = 1;                /* Advance to "waiting for the end of the pulse" */
		}
		else                             /* Second (falling) edge: end of the pulse */
		{
			/* Total ticks = overflows*65536 + (end - start), correct even across one wrap. */
			uint32_t ticks = ((uint32_t)_ovf_cnt << 16) + (uint16_t)(now - _t1);
			uint32_t us    = (ticks * _tick_ns) / 1000UL;   /* Convert ticks -> microseconds */

			SET_BIT(TCCR5B, ICES5);      /* Re-arm for a RISING edge (next pulse) */
			_state  = 0;                 /* Back to waiting for the first edge */

			if (_on_time) _on_time(us);  /* Deliver the pulse width to the user callback */
		}
	}
	else                                 /* ---- measuring period or frequency ---- */
	{
		if (_state == 0)                 /* First rising edge: start of one period */
		{
			_t1      = now;              /* Time-stamp the start */
			_ovf_cnt = 0;                /* Reset overflow count */
			_state   = 1;                /* Wait for the next rising edge */
		}
		else                             /* Second rising edge: one full period elapsed */
		{
			uint32_t ticks  = ((uint32_t)_ovf_cnt << 16) + (uint16_t)(now - _t1);  /* Period in ticks */
			uint32_t per_us = (ticks * _tick_ns) / 1000UL;                         /* Period in microseconds */

			_t1      = now;              /* Use this edge as the start of the next period */
			_ovf_cnt = 0;                /* Reset overflow count for the next period */

			if (_mode == ICU_MODE_PERIOD)            /* Period mode ... */
			{
				if (_on_time) _on_time(per_us);       /* ... report the period in microseconds */
			}
			else                                      /* Frequency mode ... */
			{
				if (_on_freq && per_us > 0)           /* ... guard against divide-by-zero */
					_on_freq(1000000UL / per_us);     /* f(Hz) = 1e6 / period(us) */
			}
		}
	}
}
