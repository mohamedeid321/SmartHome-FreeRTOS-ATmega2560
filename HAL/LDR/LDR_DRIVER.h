/*
 * LDR_DRIVER.h
 *
 * Layer  : HAL (Hardware Abstraction Layer)
 * Target : ATmega2560 @ 16MHz
 * Device : LDR (photoresistor) read through a voltage divider into the ADC.
 * Depends: MCAL/ADC.
 *
 * Why percentage, not lux:
 *   An LDR has no standard transfer function (each part differs and the
 *   response is non-linear), so reporting absolute lux would be fake
 *   precision. Instead we map the 10-bit ADC reading (0..1023) to a clean
 *   0..100% "light level", which is what applications actually use
 *   ("if light < 30%, turn the lamp on").
 *
 * Wiring & polarity:
 *   Depending on whether the LDR is the TOP or BOTTOM leg of the divider,
 *   a brighter scene can read either higher or lower on the ADC. The
 *   m_bright_is_high flag lets you set which way round your circuit is, so
 *   100% always means "brightest".
 *
 * Author : (Mohamed Eid)
 */

#ifndef LDR_DRIVER_H_          /* Include guard opening */
#define LDR_DRIVER_H_          /* Guard token */

#include <stdint.h>            /* Fixed-width integer types */
#include "ADC_DRIVER.h"        /* Sits on top of the MCAL ADC driver */

/*------------------------------------------------------------------------
 * ENUM : divider polarity (which way brightness moves the ADC reading).
 *----------------------------------------------------------------------*/
typedef enum {
	LDR_BRIGHT_IS_LOW = 0,   /* Brighter light -> LOWER ADC value  (LDR on the TOP leg) */
	LDR_BRIGHT_IS_HIGH       /* Brighter light -> HIGHER ADC value (LDR on the BOTTOM leg) */
}LDR_Polarity;

/*------------------------------------------------------------------------
 * STRUCT : LDR configuration.
 *----------------------------------------------------------------------*/
typedef struct {
	ADC_CHANNEL  m_channel;     /* ADC channel carrying the divider midpoint */
	LDR_Polarity m_polarity;    /* How brightness maps to the ADC reading */
}LDR_Config;

/*================================ APIs ==================================*/

/* Initialise the LDR (records channel + polarity). Pass NULL for the default.
 * NOTE: ADC_init must already have run (AVCC reference). */
void LDR_init(const LDR_Config* config);

/* Light level as a percentage 0..100 (100 = brightest), polarity-corrected. */
uint8_t LDR_readPercent(void);

/* Raw averaged 10-bit ADC value (0..1023), useful for your own calibration. */
uint16_t LDR_readRaw(void);

/* Ready-made default config (channel 4, brighter = higher reading). */
extern const LDR_Config LDR_DEFAULT_CONFIG;

#endif /* LDR_DRIVER_H_ */
