/*
 * LDR_DRIVER.c
 *
 * Layer  : HAL
 * Target : ATmega2560 @ 16MHz
 * Device : LDR via voltage divider on an ADC channel.
 *
 * Author : (Mohamed Eid)
 */

#include "LDR_DRIVER.h"      /* Public types, enums and prototypes */

/*------------------------------------------------------------------------
 * Saved configuration (channel + polarity).
 *----------------------------------------------------------------------*/
static LDR_Config g_ldr;       /* Private copy of the active configuration */

/*------------------------------------------------------------------------
 * DEFAULT configuration used when LDR_init() receives NULL.
 * Channel 4, and "brighter = higher reading" (LDR on the bottom leg).
 *----------------------------------------------------------------------*/
const LDR_Config LDR_DEFAULT_CONFIG = {
	.m_channel  = ADC_CHANNEL_4,        /* Default LDR on ADC channel 4 */
	.m_polarity = LDR_BRIGHT_IS_HIGH    /* Default: more light -> higher ADC value */
};

/*------------------------------------------------------------------------
 * Helper: average 8 ADC samples to steady the reading (same idea as LM35).
 *----------------------------------------------------------------------*/
static uint16_t ldr_read_avg(void)
{
	uint32_t sum = 0;                                  /* 32-bit accumulator */
	for(uint8_t i = 0; i < 8; i++)                     /* Take 8 samples */
	{
		sum += ADC_read_channel_P(g_ldr.m_channel);    /* Read the LDR channel */
	}
	return (uint16_t)(sum >> 3);                       /* Divide by 8 -> averaged value */
}

/*========================================================================
 * LDR_init : remember the channel and polarity.
 *======================================================================*/
void LDR_init(const LDR_Config* config)
{
	if(config == 0)              /* NULL-safe: use the default config */
	{
		config = &LDR_DEFAULT_CONFIG;
	}
	g_ldr = *config;             /* Keep a private copy of the configuration */
}

/*========================================================================
 * LDR_readRaw : averaged 10-bit ADC value (0..1023), no polarity applied.
 *======================================================================*/
uint16_t LDR_readRaw(void)
{
	return ldr_read_avg();       /* Hand back the raw averaged reading for calibration */
}

/*========================================================================
 * LDR_readPercent : light level 0..100 %, corrected for wiring polarity.
 *======================================================================*/
uint8_t LDR_readPercent(void)
{
	uint16_t adc = ldr_read_avg();                 /* Averaged 0..1023 reading */

	/* Scale 0..1023 -> 0..100. (adc * 100) fits in 32-bit safely. */
	uint8_t percent = (uint8_t)(((uint32_t)adc * 100UL) / 1023UL);

	/* If brighter light gives a LOWER reading, invert so 100% = brightest. */
	if(g_ldr.m_polarity == LDR_BRIGHT_IS_LOW)
	{
		percent = 100 - percent;                   /* Flip the scale */
	}
	return percent;                                /* 0 = darkest, 100 = brightest */
}
