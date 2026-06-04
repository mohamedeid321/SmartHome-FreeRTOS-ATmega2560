/*
 * EXT_INT_DRIVER.h
 *
 * Layer   : MCAL (Microcontroller Abstraction Layer)
 * Target  : ATmega2560 @ 16MHz
 * Purpose : External Interrupt driver. The ATmega2560 has 8 dedicated
 *           external interrupt lines INT0..INT7:
 *              INT0 = PD0, INT1 = PD1, INT2 = PD2, INT3 = PD3,
 *              INT4 = PE4, INT5 = PE5, INT6 = PE6, INT7 = PE7.
 *           This driver lets the upper layers register a callback that
 *           fires on a chosen edge (rising / falling / any) or on a low
 *           level, without ever touching EICRA / EICRB / EIMSK directly.
 *
 * Usage   : 1) Fill an EXT_INT_CONFIG (or pass NULL for the default).
 *           2) Call EXT_INT_init(&cfg).
 *           3) Provide a callback; it runs inside the ISR context.
 *
 * Author  : (Mohamed Eid)
 */

#ifndef EXT_INT_DRIVER_H_      /* Include guard opening: stop double inclusion */
#define EXT_INT_DRIVER_H_      /* Define the guard token */

#include <avr/io.h>            /* AVR register map (EICRA, EICRB, EIMSK, EIFR ...) */
#include <avr/interrupt.h>     /* ISR() macro and sei()/cli() global-interrupt helpers */
#include <stdint.h>            /* Fixed-width integer types */

/*------------ MACROS : single-bit helpers ------------------------------*/
#define SET_BIT(reg,bit)    ((reg)|=(1<<(bit)))    /* Set one bit HIGH */
#define CLEAR_BIT(reg,bit)  ((reg)&=~(1<<(bit)))   /* Clear one bit LOW */
#define TOGGLE_BIT(reg,bit) ((reg)^=(1<<(bit)))    /* Flip one bit */
#define CHECK_BIT(reg,bit)  ((reg)&(1<<(bit)))     /* Test one bit */

/*------------------------------------------------------------------------
 * ENUM : which external interrupt line to use.
 * Values 0..7 map directly to the INT0..INT7 bit positions in EIMSK.
 *----------------------------------------------------------------------*/
typedef enum {
	EXT_INT0 = 0,   /* INT0 on pin PD0 */
	EXT_INT1,       /* INT1 on pin PD1 */
	EXT_INT2,       /* INT2 on pin PD2 */
	EXT_INT3,       /* INT3 on pin PD3 */
	EXT_INT4,       /* INT4 on pin PE4 */
	EXT_INT5,       /* INT5 on pin PE5 */
	EXT_INT6,       /* INT6 on pin PE6 */
	EXT_INT7        /* INT7 on pin PE7 */
}EXT_INT_LINE;

/*------------------------------------------------------------------------
 * ENUM : what condition triggers the interrupt.
 * These two-bit codes are exactly the ISCn1:ISCn0 values from the datasheet.
 *----------------------------------------------------------------------*/
typedef enum {
	EXT_INT_LOW_LEVEL   = 0,   /* 00 : trigger while the pin is held LOW */
	EXT_INT_ANY_EDGE    = 1,   /* 01 : trigger on any logic change */
	EXT_INT_FALLING_EDGE= 2,   /* 10 : trigger on HIGH -> LOW transition */
	EXT_INT_RISING_EDGE = 3    /* 11 : trigger on LOW -> HIGH transition */
}EXT_INT_SENSE;

/*------------------------------------------------------------------------
 * ENUM : whether to enable the internal pull-up on the interrupt pin.
 * Buttons to GND usually want the pull-up ON (idle HIGH, pressed LOW).
 *----------------------------------------------------------------------*/
typedef enum {
	EXT_INT_NO_PULLUP = 0,   /* Leave the pin floating (external pull provided) */
	EXT_INT_PULLUP           /* Enable the internal pull-up resistor */
}EXT_INT_PULL;

/*------------------------------------------------------------------------
 * STRUCT : full configuration of one external interrupt line.
 *----------------------------------------------------------------------*/
typedef struct {
	EXT_INT_LINE  m_line;       /* Which INTx line */
	EXT_INT_SENSE m_sense;      /* Edge / level that fires it */
	EXT_INT_PULL  m_pull;       /* Internal pull-up on/off */
	void (*callback)(void);     /* Function to run when the interrupt fires (NULL = none) */
}EXT_INT_CONFIG;

/*================================ APIs ==================================*/

/* Initialise one external interrupt line. Pass NULL to use DEFAULT_CONFIG. */
void EXT_INT_init(const EXT_INT_CONFIG* config);

/* Enable a previously configured line (unmask it in EIMSK). */
void EXT_INT_enable(EXT_INT_LINE line);

/* Disable a line (mask it in EIMSK) without losing its configuration. */
void EXT_INT_disable(EXT_INT_LINE line);

/* Change the trigger condition of a line at run time. */
void EXT_INT_setSense(EXT_INT_LINE line, EXT_INT_SENSE sense);

/* Register / replace the callback of a line at run time. */
void EXT_INT_setCallback(EXT_INT_LINE line, void (*callback)(void));

#endif /* EXT_INT_DRIVER_H_ */
