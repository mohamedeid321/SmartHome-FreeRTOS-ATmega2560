/*
 * WDT_DRIVER.c
 *
 * Layer  : MCAL
 * Target : ATmega2560 @ 16MHz
 * Purpose: Implementation of the watchdog timer driver.
 *
 * Critical hardware rule (timed sequence):
 *   To CHANGE the watchdog prescaler or to TURN IT OFF you must, within the
 *   same 4-clock-cycle window:
 *     1) write WDCE=1 and WDE=1 together (unlocks the config), then
 *     2) write the new WDTCSR value.
 *   Interrupts must be disabled during this window or an ISR could blow the
 *   4-cycle budget and the change would be ignored. We use cli()/restore.
 *
 * Boot rule:
 *   If a watchdog reset just happened, WDRF in MCUSR stays set AND the
 *   watchdog may still be running with a short timeout. We snapshot MCUSR,
 *   clear it, and disable the watchdog early so it can be reconfigured safely.
 *
 * Author : (Mohamed Eid)
 */

#include "WDT_DRIVER.h"        /* Public types, enums and prototypes */

/*------------------------------------------------------------------------
 * Saved reset-source snapshot. We grab MCUSR as early as possible because
 * those flags are only meaningful right after boot.
 *----------------------------------------------------------------------*/
static uint8_t g_reset_source = 0;       /* Holds the MCUSR value captured at startup */

/*------------------------------------------------------------------------
 * Watchdog interrupt callback (used by the interrupt modes). 'volatile'
 * because the WDT ISR reads it asynchronously.
 *----------------------------------------------------------------------*/
static void (*volatile wdt_callback)(void) = 0;   /* No handler attached by default */

/*------------------------------------------------------------------------
 * DEFAULT configuration used when WDT_init() receives NULL.
 * Safe choice: ~1 second timeout, pure system-reset mode, no callback.
 *----------------------------------------------------------------------*/
static const WDT_CONFIG DEFAULT_CONFIG = {
	.m_timeout = WDT_TIMEOUT_1S,     /* Default bite time is about 1 second */
	.m_mode    = WDT_MODE_RESET,     /* Default action is a clean system reset */
	.callback  = 0                   /* Default has no interrupt handler */
};

/*========================================================================
 * WDT_getResetSource : return the boot-time MCUSR snapshot.
 *======================================================================*/
uint8_t WDT_getResetSource(void)
{
	return g_reset_source;           /* Hand the caller the flags we saved during WDT_init */
}

/*========================================================================
 * WDT_reset : "kick" the dog. Single assembly instruction wrapper.
 *======================================================================*/
void WDT_reset(void)
{
	__asm__ __volatile__ ("wdr");    /* The AVR 'wdr' instruction restarts the watchdog counter */
}

/*========================================================================
 * WDT_stop : fully disable the watchdog using the timed sequence.
 *======================================================================*/
void WDT_stop(void)
{
	uint8_t sreg = SREG;             /* Save the current global-interrupt state */
	cli();                           /* Disable interrupts so the 4-cycle window is not interrupted */

	__asm__ __volatile__ ("wdr");    /* Reset the counter first (datasheet-recommended before reconfig) */
	MCUSR &= ~(1 << WDRF);           /* Clear the watchdog-reset flag, else WDE cannot be cleared */
	WDTCSR |= (1 << WDCE) | (1 << WDE);   /* Step 1: open the timed window (WDCE+WDE together) */
	WDTCSR = 0x00;                   /* Step 2 (within 4 cycles): write 0 -> watchdog fully off */

	SREG = sreg;                     /* Restore the previous interrupt state */
}

/*========================================================================
 * WDT_init : configure and start the watchdog.
 *======================================================================*/
void WDT_init(const WDT_CONFIG* config)
{
	if(config == 0)                  /* If the caller passed NULL ... */
	{
		config = &DEFAULT_CONFIG;    /* ... use the safe default configuration */
	}

	g_reset_source = MCUSR;          /* Snapshot why we last reset (power-on/ext/brown-out/watchdog) */
	MCUSR = 0x00;                    /* Clear all reset flags now that we have saved them */

	wdt_callback = config->callback; /* Remember the interrupt-mode handler (if any) */

	/* Build the prescaler part of WDTCSR from the timeout code.
	 * The timeout enum already encodes WDP3 (bit5, value 0x20) plus WDP2:0
	 * (bits 0..2), so we can mask those bits straight out of it. */
	uint8_t prescaler = (config->m_timeout & 0x07)            /* WDP2:WDP0 (the low 3 prescaler bits) */
	                  | (config->m_timeout & 0x20);           /* WDP3 (bit 5) for the 4s / 8s ranges */

	/* Decide which control bits set the chosen mode:
	 *   WDE  -> enables the reset action.
	 *   WDIE -> enables the interrupt action. */
	uint8_t mode_bits = 0;                                    /* Start with no mode bits set */
	switch(config->m_mode)
	{
		case WDT_MODE_RESET:                                  /* Reset only */
			mode_bits = (1 << WDE);                           /* WDE on, WDIE off */
			break;
		case WDT_MODE_INTERRUPT:                              /* Interrupt only */
			mode_bits = (1 << WDIE);                          /* WDIE on, WDE off */
			break;
		case WDT_MODE_INTERRUPT_RESET:                        /* Interrupt first, reset on the next timeout */
			mode_bits = (1 << WDIE) | (1 << WDE);             /* Both on */
			break;
	}

	uint8_t sreg = SREG;             /* Save global interrupt state */
	cli();                           /* Disable interrupts: protect the 4-cycle timed window */

	__asm__ __volatile__ ("wdr");    /* Reset the counter before reconfiguring */
	WDTCSR |= (1 << WDCE) | (1 << WDE);          /* Step 1: open the timed configuration window */
	WDTCSR = mode_bits | prescaler;              /* Step 2 (within 4 cycles): write mode + prescaler atomically */

	SREG = sreg;                     /* Restore the previous interrupt state */

	if(config->m_mode != WDT_MODE_RESET)         /* If an interrupt mode was selected ... */
	{
		sei();                                   /* ... make sure the global interrupt flag is on */
	}
}

/*========================================================================
 * Watchdog ISR : runs in the interrupt modes. Forwards to the callback.
 * Note: in INTERRUPT mode the hardware auto-clears WDIE after firing, so
 * to keep getting interrupts the handler would re-arm it; for
 * INTERRUPT+RESET we deliberately leave it so the next timeout resets.
 *======================================================================*/
ISR(WDT_vect)
{
	if(wdt_callback)                 /* If the application registered a handler ... */
	{
		wdt_callback();              /* ... call it (e.g. save state before the imminent reset) */
	}
}
