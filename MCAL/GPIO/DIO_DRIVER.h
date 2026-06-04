/*
 * DIO_DRIVER.h
 *
 * Layer   : MCAL (Microcontroller Abstraction Layer)
 * Target  : ATmega2560 @ 16MHz
 * Purpose : Generic Digital I/O driver. Abstracts the raw AVR port
 *           registers (DDRx / PORTx / PINx) behind a clean, typed API
 *           so the upper layers (HAL / APP) never touch registers directly.
 *
 * Author  : (Mohamed Eid)
 */

#ifndef DIO_DRIVER_H_          /* Include guard: prevents this header being included twice in one translation unit */
#define DIO_DRIVER_H_          /* Define the guard symbol so the second inclusion is skipped by the preprocessor */

#include <avr/io.h>            /* Brings in the AVR register definitions (DDRA, PORTA, PINA, ... ) for ATmega2560 */
#include <stdint.h>            /* Brings in fixed-width integer types (uint8_t) used across the API */

/*------------ MACROS : single-bit manipulation helpers -----------------*/
#define SET_BIT(reg,bit)    ((reg)|=(1<<(bit)))      /* Force one bit HIGH inside a register without touching the others */
#define CLEAR_BIT(reg,bit)  ((reg)&=~(1<<(bit)))     /* Force one bit LOW  inside a register without touching the others */
#define TOGGLE_BIT(reg,bit) ((reg)^=(1<<(bit)))      /* Flip one bit (HIGH<->LOW) inside a register */
#define CHECK_BIT(reg,bit)  ((reg)&(1<<(bit)))       /* Read one bit: returns non-zero if the bit is HIGH, zero if LOW */

/*------------------------------------------------------------------------
 * ENUM : which physical port we are addressing.
 * ATmega2560 exposes ports A..L (no I). We list the ones brought to pins.
 *----------------------------------------------------------------------*/
typedef enum {
	DIO_PORTA = 0,   /* Port A */
	DIO_PORTB,       /* Port B */
	DIO_PORTC,       /* Port C */
	DIO_PORTD,       /* Port D */
	DIO_PORTE,       /* Port E */
	DIO_PORTF,       /* Port F (also the ADC channels 0..7) */
	DIO_PORTG,       /* Port G (partial port, 6 pins) */
	DIO_PORTH,       /* Port H */
	DIO_PORTJ,       /* Port J */
	DIO_PORTK,       /* Port K (also ADC channels 8..15) */
	DIO_PORTL        /* Port L */
}DIO_PORT;

/*------------------------------------------------------------------------
 * ENUM : pin index inside a port (0..7).
 *----------------------------------------------------------------------*/
typedef enum {
	DIO_PIN0 = 0,    /* Bit 0 of the chosen port */
	DIO_PIN1,        /* Bit 1 */
	DIO_PIN2,        /* Bit 2 */
	DIO_PIN3,        /* Bit 3 */
	DIO_PIN4,        /* Bit 4 */
	DIO_PIN5,        /* Bit 5 */
	DIO_PIN6,        /* Bit 6 */
	DIO_PIN7         /* Bit 7 */
}DIO_PIN;

/*------------------------------------------------------------------------
 * ENUM : pin direction. INPUT optionally with the internal pull-up enabled.
 *----------------------------------------------------------------------*/
typedef enum {
	DIO_INPUT = 0,         /* High-impedance input, no pull-up (floating) */
	DIO_INPUT_PULLUP,      /* Input with the internal ~50k pull-up resistor enabled */
	DIO_OUTPUT             /* Push-pull output */
}DIO_DIRECTION;

/*------------------------------------------------------------------------
 * ENUM : logic level for reads and writes.
 *----------------------------------------------------------------------*/
typedef enum {
	DIO_LOW = 0,     /* Logic 0 / GND level */
	DIO_HIGH         /* Logic 1 / VCC level */
}DIO_LEVEL;

/*------------------------------------------------------------------------
 * STRUCT : full configuration of a single pin.
 * The user fills one of these and hands its address to DIO_init().
 *----------------------------------------------------------------------*/
typedef struct {
	DIO_PORT      m_port;        /* Which port the pin lives on */
	DIO_PIN       m_pin;         /* Which bit inside that port */
	DIO_DIRECTION m_direction;   /* Input / input-pullup / output */
	DIO_LEVEL     m_init_level;  /* Initial output level (ignored for inputs) */
}DIO_CONFIG;

/*================================ APIs ==================================*/

/* Configure one pin (direction + initial state). Pass NULL to use the DEFAULT_CONFIG below. */
void DIO_init(const DIO_CONFIG* config);

/* Change a pin direction at run time (e.g. switch a bus line from in to out). */
void DIO_setDirection(DIO_PORT port, DIO_PIN pin, DIO_DIRECTION dir);

/* Drive an OUTPUT pin to a given level. */
void DIO_writePin(DIO_PORT port, DIO_PIN pin, DIO_LEVEL level);

/* Toggle an OUTPUT pin (HIGH becomes LOW and vice-versa). */
void DIO_togglePin(DIO_PORT port, DIO_PIN pin);

/* Read the current level of any pin (input or output). Returns DIO_LOW / DIO_HIGH. */
DIO_LEVEL DIO_readPin(DIO_PORT port, DIO_PIN pin);

/* Write all 8 bits of a port at once (fast bus / parallel-data use). */
void DIO_writePort(DIO_PORT port, uint8_t value);

/* Read all 8 bits of a port at once. */
uint8_t DIO_readPort(DIO_PORT port);

#endif /* DIO_DRIVER_H_ */
