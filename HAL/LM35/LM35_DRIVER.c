/*
 * LM35_DRIVER.c
 *
 * Layer  : HAL
 * Target : ATmega2560 @ 16MHz
 * Device : LM35 temperature sensor on an ADC channel.
 *
 * Author : (Mohamed Eid)
 */

#include "LM35_DRIVER.h"     /* Public types, enums and prototypes */

/*------------------------------------------------------------------------
 * Saved configuration (which channel the LM35 is on).
 *----------------------------------------------------------------------*/
static LM35_Config g_lm35;     /* Private copy of the active configuration */

/*------------------------------------------------------------------------
 * DEFAULT configuration used when LM35_init() receives NULL.
 * Channel 3 keeps it clear of any control pins on lower ADC channels.
 *----------------------------------------------------------------------*/
const LM35_Config LM35_DEFAULT_CONFIG = {
	.m_channel = ADC_CHANNEL_3     /* Default LM35 on ADC channel 3 */
};

/*------------------------------------------------------------------------
 * Helper: average several ADC samples to smooth out noise. The LM35 output
 * is small (mV), so a couple of stray counts matter; averaging 8 reads
 * gives a steadier value without floating point.
 *----------------------------------------------------------------------*/
static uint16_t lm35_read_avg(void)
{
	uint32_t sum = 0;                                   /* Accumulator (32-bit to hold 8 x up-to-1023) */
	for(uint8_t i = 0; i < 8; i++)                      /* Take 8 samples */
	{
		sum += ADC_read_channel_P(g_lm35.m_channel);    /* Read the LM35 channel each time */
	}
	return (uint16_t)(sum >> 3);                        /* Divide by 8 (>>3) -> the averaged reading */
}

/*========================================================================
 * LM35_init : remember the channel the sensor is wired to.
 *======================================================================*/
void LM35_init(const LM35_Config* config)
{
	if(config == 0)              /* NULL-safe: use the default channel */
	{
		config = &LM35_DEFAULT_CONFIG;
	}
	g_lm35 = *config;            /* Keep a private copy of the configuration */
}

/*========================================================================
 * LM35_readCx10 : temperature x10 (preserves one decimal place).
 *   adc * 500 / 1024  gives whole degrees; multiply the numerator by 10
 *   first to keep one decimal, i.e. adc * 5000 / 1024.
 *   We use 32-bit math so adc(<=1023) * 5000 does not overflow.
 *======================================================================*/
uint16_t LM35_readCx10(void)
{
	uint16_t adc = lm35_read_avg();                     /* Averaged 10-bit reading */
	uint32_t tx10 = ((uint32_t)adc * 5000UL) / 1024UL;  /* Temperature * 10 (e.g. 253 = 25.3 C) */
	return (uint16_t)tx10;                              /* Return the scaled value */
}

/*========================================================================
 * LM35_readC : temperature in whole degrees Celsius.
 *======================================================================*/
uint16_t LM35_readC(void)
{
	uint16_t adc = lm35_read_avg();                     /* Averaged 10-bit reading */
	uint32_t t = ((uint32_t)adc * 500UL) / 1024UL;      /* Whole degrees (e.g. 25) */
	return (uint16_t)t;                                /* Return the temperature */
}
