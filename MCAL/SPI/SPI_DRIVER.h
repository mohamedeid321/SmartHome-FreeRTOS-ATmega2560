/*
 * SPI_DRIVER.h
 *
 * Layer  : MCAL
 * Target : ATmega2560 @ 16MHz
 * Pins   : SS = PB0, SCK = PB1, MOSI = PB2, MISO = PB3.
 *          (Note: these differ from the ATmega32, where they were PB4..PB7.)
 * Purpose: SPI master driver, adapted from the author's ATmega32 code.
 *          Register names (SPCR, SPSR, SPDR) are identical on the 2560;
 *          only the port-B pin numbers change.
 *
 * Author : (Mohamed Eid)
 */

#ifndef SPI_DRIVER_H_          /* Include guard opening */
#define SPI_DRIVER_H_          /* Guard token */

#include <avr/io.h>            /* AVR register map (SPCR, SPSR, SPDR) */
#include <avr/interrupt.h>     /* ISR() macro for interrupt mode */
#include <stdint.h>            /* Fixed-width integer types */

#ifndef NULL
#define NULL 0                 /* Provide NULL if missing */
#endif

/*------------ MACROS : single-bit helpers ------------------------------*/
#define SET_BIT(reg,bit)    ((reg)|=(1<<(bit)))    /* Set one bit HIGH */
#define CLEAR_BIT(reg,bit)  ((reg)&=~(1<<(bit)))   /* Clear one bit LOW */
#define CHECK_BIT(reg,bit)  ((reg)&(1<<(bit)))     /* Test one bit */

/*------------ SPI pin map on ATmega2560 (PORTB) ------------------------*/
#define SPI_SS_PIN    PB0      /* Slave Select (output, idle HIGH) */
#define SPI_SCK_PIN   PB1      /* Serial Clock (output) */
#define SPI_MOSI_PIN  PB2      /* Master Out Slave In (output) */
#define SPI_MISO_PIN  PB3      /* Master In Slave Out (input) */

/*------------------------------------------------------------------------
 * ENUM : clock polarity / phase (SPI modes 0..3).
 *----------------------------------------------------------------------*/
typedef enum {
	SPI_CLOCK_MODE_0,   /* CPOL=0 CPHA=0 (most common, default) */
	SPI_CLOCK_MODE_1,   /* CPOL=0 CPHA=1 */
	SPI_CLOCK_MODE_2,   /* CPOL=1 CPHA=0 */
	SPI_CLOCK_MODE_3    /* CPOL=1 CPHA=1 */
}SPI_CLOCK_MODE;

/*------------------------------------------------------------------------
 * ENUM : SCK frequency = F_CPU / divider. The encoded value packs SPR1:SPR0
 * (in SPCR) and the SPI2X bit (in SPSR); the driver splits them out.
 *----------------------------------------------------------------------*/
typedef enum {
	SPI_RATE_2   = 4,   /* F_CPU/2  (fastest, uses SPI2X) */
	SPI_RATE_4   = 0,   /* F_CPU/4 */
	SPI_RATE_16  = 1,   /* F_CPU/16 */
	SPI_RATE_64  = 2,   /* F_CPU/64 (safe default) */
	SPI_RATE_128 = 3    /* F_CPU/128 (slowest) */
}SPI_RATE;

/*------------------------------------------------------------------------
 * ENUM : polling vs interrupt transfer completion.
 *----------------------------------------------------------------------*/
typedef enum {
	SPI_MODE_POLLING,    /* Block until the byte is shifted */
	SPI_MODE_INTERRUPT   /* Completion delivered via callback */
}SPI_MODE;

/*------------------------------------------------------------------------
 * STRUCT : full SPI master configuration.
 *----------------------------------------------------------------------*/
typedef struct {
	SPI_CLOCK_MODE m_clock_mode;   /* SPI mode 0..3 */
	SPI_RATE       m_rate;         /* SCK divider */
	SPI_MODE       m_mode;         /* Polling or interrupt */
	void (*callback)(uint8_t);     /* Completion callback for interrupt mode (NULL = none) */
}SPI_CONFIG;

/*================================ APIs ==================================*/

/* Initialise SPI as master. Pass NULL to use SPI_DEFAULT_CONFIG. */
void SPI_init(const SPI_CONFIG* config);

/* Full-duplex transfer: send 'data', return the byte received (polling mode). */
uint8_t SPI_transfer(uint8_t data);

/* Drive the Slave-Select line LOW (select) or HIGH (deselect) manually. */
void SPI_select(void);     /* SS = LOW : begin talking to the slave */
void SPI_deselect(void);   /* SS = HIGH: end the transaction */

/* Ready-made default config (mode 0, F_CPU/64, polling). */
extern const SPI_CONFIG SPI_DEFAULT_CONFIG;

#endif /* SPI_DRIVER_H_ */
