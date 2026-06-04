/*
 * ADC_DRIVER.c
 *
 * Layer  : MCAL
 * Target : ATmega2560 @ 16MHz
 * Purpose: ADC driver implementation, adapted from the author's ATmega32 code.
 *
 * Key 2560 difference vs ATmega32:
 *   The channel selector is 5 bits. MUX4:MUX0 live in ADMUX, but MUX5 lives
 *   in ADCSRB. Channels 0..7 keep MUX5 = 0; channels 8..15 need MUX5 = 1.
 *   The helper _select_channel() handles this split.
 *
 * Author : (Mohamed Eid)
 */

#include "ADC_DRIVER.h"        /* Public types, enums and prototypes */

static void (*adc_callback)(uint16_t) = 0;   /* Interrupt-mode result callback (NULL = none) */

/*------------------------------------------------------------------------
 * DEFAULT configuration used when ADC_init() receives NULL.
 *----------------------------------------------------------------------*/
const ADC_CONFIG_SELCECTION ADC_DEFAULT_CONFIG = {
	.m_channel = ADC_CHANNEL_0,      /* Default channel 0 (PF0) */
	.m_mode    = ADC_MODE_POLLING,   /* Default polling mode */
	.m_vref    = ADC_VREF_AVCC,      /* Default reference is AVCC (~5V) */
	.m_pre     = ADC_PRESCALER_128,  /* /128 -> 125 kHz ADC clock at 16 MHz */
	.callback  = 0                   /* No callback by default */
};

/*------------------------------------------------------------------------
 * Helper: route a channel 0..15 into the MUX bits, splitting MUX5 out to
 * ADCSRB as the 2560 hardware requires.
 *----------------------------------------------------------------------*/
static void _select_channel(ADC_CHANNEL channel)
{
	if(channel <= ADC_CHANNEL_7)                  /* Channels 0..7 use PORTF and MUX5 = 0 */
	{
		CLEAR_BIT(ADCSRB, MUX5);                  /* Clear MUX5 (selects the low channel bank) */
		ADMUX = (ADMUX & 0xE0) | (channel & 0x1F);/* Keep REFS/ADLAR, set MUX4:MUX0 to the channel */
	}
	else                                          /* Channels 8..15 use PORTK and MUX5 = 1 */
	{
		SET_BIT(ADCSRB, MUX5);                    /* Set MUX5 (selects the high channel bank) */
		ADMUX = (ADMUX & 0xE0) | (channel & 0x07);/* Lower 3 bits select within the high bank */
	}
}

/*========================================================================
 * ADC_init : configure reference, mode, prescaler and remember the callback.
 *======================================================================*/
void ADC_init(const ADC_CONFIG_SELCECTION* config)
{
	if(config == 0)                  /* NULL-safe: use the default config if none given */
	{
		config = &ADC_DEFAULT_CONFIG;
	}
	adc_vref_selection(config->m_vref);        /* 1) Set the voltage reference (REFS1:REFS0) */
	adc_mode_selection(config->m_mode);        /* 2) Enable the ADC, choose polling/interrupt */
	adc_prescaler_selection(config->m_pre);    /* 3) Set the ADC clock prescaler */
	adc_callback = config->callback;           /* 4) Save the interrupt-mode callback */
}

/*========================================================================
 * ADC_read_channel_P : blocking single read (polling).
 *======================================================================*/
uint16_t ADC_read_channel_P(ADC_CHANNEL channel)
{
	_select_channel(channel);            /* Point the MUX at the requested channel (handles MUX5) */
	start_conversion();                  /* Kick off one conversion */
	while(CHECK_BIT(ADCSRA, ADSC));      /* Wait here until ADSC clears = conversion finished */
	return ADCW;                         /* Return the 10-bit result (ADCW = ADCL + ADCH combined) */
}

/*========================================================================
 * start_conversion : trigger a single conversion.
 *======================================================================*/
void start_conversion(void)
{
	SET_BIT(ADCSRA, ADSC);               /* ADSC = 1 starts a conversion; hardware clears it when done */
}

/*========================================================================
 * ADC_read_channel_INR : start an interrupt-driven conversion.
 *======================================================================*/
void ADC_read_channel_INR(ADC_CHANNEL channel)
{
	_select_channel(channel);            /* Select the channel (handles MUX5) */
	start_conversion();                  /* Start; the ISR will deliver the result via callback */
}

/*========================================================================
 * adc_mode_selection : enable the ADC and pick polling vs interrupt.
 *======================================================================*/
void adc_mode_selection(ADC_MODE mode)
{
	switch(mode)
	{
		case ADC_MODE_POLLING:                       /* Polling: ADC on, interrupt off */
			SET_BIT(ADCSRA, ADEN);                   /* ADEN = 1 powers up the ADC */
			CLEAR_BIT(ADCSRA, ADIE);                 /* ADIE = 0 disables the conversion-complete interrupt */
			break;
		case ADC_MODE_INTERRUPT:                     /* Interrupt: ADC on, interrupt on */
			SET_BIT(ADCSRA, ADEN);                   /* Power up the ADC */
			SET_BIT(ADCSRA, ADIE);                   /* Enable the conversion-complete interrupt */
			sei();                                   /* Global interrupt enable */
			break;
	}
}

/*========================================================================
 * adc_vref_selection : program REFS1:REFS0 (bits 7:6 of ADMUX).
 *======================================================================*/
void adc_vref_selection(ADC_VREF vref)
{
	ADMUX = (ADMUX & 0x3F) | ((vref & 0x03) << 6);   /* Keep MUX bits, overwrite the 2 reference bits */
}

/*========================================================================
 * adc_prescaler_selection : program ADPS2:ADPS0 (low 3 bits of ADCSRA).
 *======================================================================*/
void adc_prescaler_selection(ADC_PRESCALER pre)
{
	ADCSRA = (ADCSRA & 0xF8) | (pre & 0x07);         /* Keep the other ADCSRA bits, set the 3 prescaler bits */
}

/*========================================================================
 * ISR - conversion complete: forward the result, then restart (free-run).
 *======================================================================*/
ISR(ADC_vect)
{
	if(adc_callback != 0)                /* If a callback is registered ... */
	{
		adc_callback(ADCW);              /* ... hand it the fresh 10-bit reading */
	}
	start_conversion();                  /* Start the next conversion for continuous sampling */
}
