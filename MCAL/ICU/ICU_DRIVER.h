/*
 * ICU_DRIVER.h
 *
 * Layer   : MCAL (Microcontroller Abstraction Layer)
 * Target  : ATmega2560 @ 16MHz
 * Timer   : Timer5 (16-bit). NOTE: on the 2560 we deliberately use Timer5
 *           for the ICU because FreeRTOS owns Timer1 for its system tick.
 *           Using Timer1 here would clash with the RTOS.
 * Capture pin : ICP5 = PL1  (Arduino Mega "Digital Pin 48")
 *
 * Purpose : Input Capture Unit driver. Time-stamps external edges on ICP5
 *           to measure:
 *             - Pulse Width   (e.g. HC-SR04 ultrasonic echo)
 *             - Period        (signal timing)
 *             - Frequency     (RPM, square-wave inputs)
 *
 * Style   : keeps the original author's config-struct + DEFAULT_CONFIG
 *           pattern, AUTO prescaler selection, and callback-based results.
 *
 * Author  : (Mohamed Eid)
 */

#ifndef ICU_DRIVER_H_          /* Include guard opening */
#define ICU_DRIVER_H_          /* Guard token */

#ifndef F_CPU                  /* Allow the build system to define F_CPU first */
#define F_CPU 16000000UL       /* Default to 16 MHz (the 2560 board clock) if not set */
#endif

#include <avr/io.h>            /* AVR register map (TCCR5A/B, ICR5, TIMSK5 ...) */
#include <avr/interrupt.h>     /* ISR() macro and sei() */
#include <stdint.h>            /* Fixed-width integer types */

/*------------ MACROS : single-bit helpers ------------------------------*/
#define SET_BIT(reg,bit)    ((reg)|=(1<<(bit)))    /* Set one bit HIGH */
#define CLEAR_BIT(reg,bit)  ((reg)&=~(1<<(bit)))   /* Clear one bit LOW */
#define TOGGLE_BIT(reg,bit) ((reg)^=(1<<(bit)))    /* Flip one bit */
#define CHECK_BIT(reg,bit)  ((reg)&(1<<(bit)))     /* Test one bit */

/*------------------------------------------------------------------------
 * ENUM : what to measure. Pick one; the driver computes it for you.
 *----------------------------------------------------------------------*/
typedef enum {
	ICU_MODE_PULSE_WIDTH,  /* rising -> falling : pulse width  -> on_time_us (microseconds) */
	ICU_MODE_PERIOD,       /* rising -> rising  : period       -> on_time_us (microseconds) */
	ICU_MODE_FREQUENCY     /* rising -> rising  : frequency     -> on_freq_hz (Hz) */
}ICU_Mode;

/*------------------------------------------------------------------------
 * ENUM : Timer5 clock prescaler. AUTO lets the driver pick a sensible one
 * from F_CPU so a single tick is about 0.5 microsecond at 16 MHz.
 *----------------------------------------------------------------------*/
typedef enum {
	ICU_PRESCALER_AUTO = 0,    /* Driver chooses the prescaler automatically (recommended) */
	ICU_PRESCALER_1    = 1,    /* clk / 1 */
	ICU_PRESCALER_8    = 2,    /* clk / 8 */
	ICU_PRESCALER_64   = 3,    /* clk / 64 */
	ICU_PRESCALER_256  = 4,    /* clk / 256 */
	ICU_PRESCALER_1024 = 5     /* clk / 1024 */
}ICU_Prescaler;

/*------------------------------------------------------------------------
 * ENUM : input noise canceler. ON samples the pin 4x to reject glitches.
 *----------------------------------------------------------------------*/
typedef enum {
	ICU_NC_OFF = 0,    /* No filtering (fastest response) */
	ICU_NC_ON  = 1     /* 4-sample filter, rejects false captures from noise */
}ICU_NoiseCancel;

/*------------------------------------------------------------------------
 * STRUCT : driver configuration.
 *   - use on_time_us for PULSE_WIDTH and PERIOD modes.
 *   - use on_freq_hz for FREQUENCY mode.
 *----------------------------------------------------------------------*/
typedef struct {
	ICU_Mode        mode;             /* Which measurement to perform */
	ICU_Prescaler   prescaler;        /* AUTO recommended */
	ICU_NoiseCancel noise_cancel;     /* Glitch filter on/off */
	void (*on_time_us)(uint32_t us);  /* Result callback for PULSE_WIDTH / PERIOD */
	void (*on_freq_hz)(uint32_t hz);  /* Result callback for FREQUENCY */
}ICU_Config;

/*================================ APIs ==================================*/

/* Ready-made default config: pulse-width mode, AUTO prescaler, noise canceler ON,
 * no callbacks. Copy it and just set the mode and callback you need. */
extern const ICU_Config ICU_DEFAULT_CONFIG;

/* Initialise the ICU on Timer5. Pass NULL to use ICU_DEFAULT_CONFIG. */
void ICU_init(const ICU_Config *cfg);

/* Stop measuring and free Timer5. */
void ICU_stop(void);

#endif /* ICU_DRIVER_H_ */
