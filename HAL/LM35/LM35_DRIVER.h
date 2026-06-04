/*
 * LM35_DRIVER.h
 *
 * Layer  : HAL (Hardware Abstraction Layer)
 * Target : ATmega2560 @ 16MHz
 * Device : LM35 analog temperature sensor (10 mV per degree Celsius).
 * Depends: MCAL/ADC.
 *
 * Conversion (with AVCC = 5V reference, 10-bit ADC):
 *   Vin(mV)   = adc * 5000 / 1024
 *   Temp(C)   = Vin / 10   =  adc * 5000 / 1024 / 10  =  adc * 500 / 1024
 * We keep everything in integer math (no float, which is heavy on AVR) and
 * also offer a "temp x10" reading to preserve one decimal place.
 *
 * Author : (Mohamed Eid)
 */

#ifndef LM35_DRIVER_H_         /* Include guard opening */
#define LM35_DRIVER_H_         /* Guard token */

#include <stdint.h>            /* Fixed-width integer types */
#include "ADC_DRIVER.h"        /* Sits on top of the MCAL ADC driver */

/*------------------------------------------------------------------------
 * STRUCT : which ADC channel the LM35 output is wired to.
 *----------------------------------------------------------------------*/
typedef struct {
	ADC_CHANNEL m_channel;     /* ADC channel carrying the LM35 Vout */
}LM35_Config;

/*================================ APIs ==================================*/

/* Initialise the LM35 (records the channel). Pass NULL for LM35_DEFAULT_CONFIG.
 * NOTE: the ADC peripheral itself must already be initialised (ADC_init)
 * with an AVCC (5V) reference before reading. */
void LM35_init(const LM35_Config* config);

/* Read the temperature in whole degrees Celsius (e.g. 25). */
uint16_t LM35_readC(void);

/* Read the temperature x10 (e.g. 253 means 25.3 C) for one decimal place. */
uint16_t LM35_readCx10(void);

/* Ready-made default config (channel 3). */
extern const LM35_Config LM35_DEFAULT_CONFIG;

#endif /* LM35_DRIVER_H_ */
