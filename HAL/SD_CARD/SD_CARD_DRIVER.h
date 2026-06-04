/*
 * SD_CARD_DRIVER.h
 *
 * Layer  : HAL (Hardware Abstraction Layer)
 * Target : ATmega2560 @ 16MHz
 * Device : SD / microSD card in SPI mode (block-level, no filesystem).
 * Depends: MCAL/SPI and MCAL/DIO.
 *
 * Scope:
 *   This is a raw BLOCK driver: it initialises the card into SPI mode and
 *   reads / writes whole 512-byte blocks addressed by block number. There is
 *   intentionally NO FAT filesystem - that would be large and RAM-hungry. For
 *   data logging you simply pick block numbers to store records in, which is
 *   plenty for this project and far simpler to reason about.
 *
 * Electrical note:
 *   SD cards are 3.3V devices. On a 5V board use a level shifter (or a card
 *   module that includes one). In Proteus this is not an issue.
 *
 * Author : (Mohamed Eid)
 */

#ifndef SD_CARD_DRIVER_H_      /* Include guard opening */
#define SD_CARD_DRIVER_H_      /* Guard token */

#include <stdint.h>            /* Fixed-width integer types */
#include "SPI_DRIVER.h"        /* Talks to the card over SPI */
#include "DIO_DRIVER.h"        /* Drives the chip-select pin */

/* A standard SD data block is 512 bytes. */
#define SD_BLOCK_SIZE   512U

/*------------------------------------------------------------------------
 * ENUM : status codes returned by the driver.
 *----------------------------------------------------------------------*/
typedef enum {
	SD_OK = 0,            /* Operation succeeded */
	SD_ERR_INIT,          /* Card did not enter SPI/idle mode */
	SD_ERR_TIMEOUT,       /* Card stopped responding */
	SD_ERR_RESPONSE,      /* Unexpected response token */
	SD_ERR_PARAM          /* Bad argument (e.g. NULL buffer) */
}SD_Status;

/*------------------------------------------------------------------------
 * STRUCT : configuration - which pin is the card's chip select (CS).
 * (SCK/MOSI/MISO are owned by the SPI driver.)
 *----------------------------------------------------------------------*/
typedef struct {
	DIO_PORT m_cs_port;    /* Port of the SD chip-select pin */
	DIO_PIN  m_cs_pin;     /* Pin index of CS */
}SD_Config;

/*================================ APIs ==================================*/

/* Power-up + initialise the card into SPI mode. SPI_init must run first.
 * Pass NULL to use SD_DEFAULT_CONFIG. Returns SD_OK on success. */
SD_Status SD_init(const SD_Config* config);

/* Read one 512-byte block (by block number) into 'buffer'. */
SD_Status SD_readBlock(uint32_t block_number, uint8_t* buffer);

/* Write one 512-byte block (by block number) from 'buffer'. */
SD_Status SD_writeBlock(uint32_t block_number, const uint8_t* buffer);

/* Ready-made default config (CS on PB0). */
extern const SD_Config SD_DEFAULT_CONFIG;

#endif /* SD_CARD_DRIVER_H_ */
