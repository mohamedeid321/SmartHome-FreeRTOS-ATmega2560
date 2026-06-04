/*
 * ADC_DRIVER.h
 *
 * Layer  : MCAL
 * Target : ATmega2560 @ 16MHz
 * Pins   : ADC0..ADC7 on PORTF, ADC8..ADC15 on PORTK.
 * Notes  : Adapted from the author's ATmega32 ADC driver. The 2560 ADC is
 *          almost identical, with two differences handled here:
 *            1) Channels 8..15 need the MUX5 bit, which lives in ADCSRB.
 *            2) The auto-trigger source bits also live in ADCSRB.
 *
 * Author : (Mohamed Eid)
 */

#ifndef ADC_DRIVER_H_          /* Include guard opening */
#define ADC_DRIVER_H_          /* Guard token */

#include <avr/io.h>            /* AVR register map (ADMUX, ADCSRA, ADCSRB, ADCW ...) */
#include <avr/interrupt.h>     /* ISR() macro for interrupt mode */
#include <stdint.h>            /* Fixed-width integer types */

/*------------ MACROS : single-bit helpers ------------------------------*/
#define SET_BIT(reg,bit)    ((reg)|=(1<<(bit)))    /* Set one bit HIGH */
#define CLEAR_BIT(reg,bit)  ((reg)&=~(1<<(bit)))   /* Clear one bit LOW */
#define TOGGLE_BIT(reg,bit) ((reg)^=(1<<(bit)))    /* Flip one bit */
#define CHECK_BIT(reg,bit)  ((reg)&(1<<(bit)))     /* Test one bit */

/*------------------------------------------------------------------------
 * ENUM : analog input channel. The 2560 has 16 channels (0..15).
 *----------------------------------------------------------------------*/
typedef enum {
	ADC_CHANNEL_0  = 0,  ADC_CHANNEL_1,  ADC_CHANNEL_2,  ADC_CHANNEL_3,    /* PF0..PF3 */
	ADC_CHANNEL_4,       ADC_CHANNEL_5,  ADC_CHANNEL_6,  ADC_CHANNEL_7,    /* PF4..PF7 */
	ADC_CHANNEL_8,       ADC_CHANNEL_9,  ADC_CHANNEL_10, ADC_CHANNEL_11,   /* PK0..PK3 */
	ADC_CHANNEL_12,      ADC_CHANNEL_13, ADC_CHANNEL_14, ADC_CHANNEL_15    /* PK4..PK7 */
}ADC_CHANNEL;

/*------------------------------------------------------------------------
 * ENUM : conversion driving style.
 *----------------------------------------------------------------------*/
typedef enum {
	ADC_MODE_POLLING,    /* Blocking read: start, wait, return result */
	ADC_MODE_INTERRUPT   /* Free-running with a callback delivering each result */
}ADC_MODE;

/*------------------------------------------------------------------------
 * ENUM : clock prescaler (ADC clock = F_CPU / prescaler). For a 16 MHz CPU,
 * /128 gives 125 kHz which is inside the 50-200 kHz recommended range for
 * full 10-bit accuracy.
 *----------------------------------------------------------------------*/
typedef enum {
	ADC_PRESCALER_2   = 1,
	ADC_PRESCALER_4   = 2,
	ADC_PRESCALER_8   = 3,
	ADC_PRESCALER_16  = 4,
	ADC_PRESCALER_32  = 5,
	ADC_PRESCALER_64  = 6,
	ADC_PRESCALER_128 = 7    /* Recommended at 16 MHz */
}ADC_PRESCALER;

/*------------------------------------------------------------------------
 * ENUM : voltage reference selection (REFS1:REFS0 in ADMUX).
 *----------------------------------------------------------------------*/
typedef enum {
	ADC_VREF_EXTRNAL = 0,    /* 00 : external AREF pin */
	ADC_VREF_AVCC    = 1,    /* 01 : AVCC (~5V) - recommended for most projects (e.g. LM35, LDR) */
	ADC_VREF_2_56V   = 3     /* 11 : internal 2.56V (note: 2560 internal ref is 2.56V, not 2.5V) */
}ADC_VREF;

/*------------------------------------------------------------------------
 * STRUCT : full ADC configuration.
 *----------------------------------------------------------------------*/
typedef struct {
	ADC_CHANNEL   m_channel;       /* Default channel (used by interrupt mode) */
	ADC_MODE      m_mode;          /* Polling or interrupt */
	ADC_VREF      m_vref;          /* Voltage reference */
	ADC_PRESCALER m_pre;           /* Clock prescaler */
	void (*callback)(uint16_t);    /* Result callback for interrupt mode (NULL = none) */
}ADC_CONFIG_SELCECTION;

/*================================ APIs ==================================*/

/* Initialise the ADC. Pass NULL to use ADC_DEFAULT_CONFIG. */
void ADC_init(const ADC_CONFIG_SELCECTION* config);

/* Polling read of one channel: returns the 10-bit result (0..1023). */
uint16_t ADC_read_channel_P(ADC_CHANNEL channel);

/* Interrupt read: starts a conversion on a channel; result comes via callback. */
void ADC_read_channel_INR(ADC_CHANNEL channel);

/* Ready-made default config (channel 0, polling, AVCC ref, /128). */
extern const ADC_CONFIG_SELCECTION ADC_DEFAULT_CONFIG;

/* Low-level helpers (kept public to match the author's original API). */
void start_conversion(void);
void adc_mode_selection(ADC_MODE mode);
void adc_vref_selection(ADC_VREF vref);
void adc_prescaler_selection(ADC_PRESCALER pre);

#endif /* ADC_DRIVER_H_ */
