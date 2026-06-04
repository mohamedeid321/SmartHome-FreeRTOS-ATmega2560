/*
 * EEPROM_DRIVER.h
 *
 * Layer   : MCAL (Microcontroller Abstraction Layer)
 * Target  : ATmega2560 @ 16MHz
 * Purpose : Internal EEPROM driver. The ATmega2560 has 4096 bytes of on-chip
 *           EEPROM (addresses 0x000..0xFFF) that survive power loss. This
 *           driver gives byte / block / typed access behind a clean API so
 *           the upper layers can persist settings (temperature threshold,
 *           keypad password, last system mode ...) without touching the
 *           EEAR / EEDR / EECR registers directly.
 *
 * Notes   : - A byte write takes ~3.3 ms; the driver polls until ready.
 *           - EEPROM endurance is ~100,000 erase/write cycles per cell, so
 *             EEPROM_update() writes only when the value actually changed,
 *             which is the safe way to extend cell life.
 *
 * Author  : (Mohamed Eid)
 */

#ifndef EEPROM_DRIVER_H_       /* Include guard opening */
#define EEPROM_DRIVER_H_       /* Guard token */

#include <avr/io.h>            /* AVR register map (EEAR, EEDR, EECR ...) */
#include <avr/interrupt.h>     /* cli()/sei() to protect the timed write start */
#include <stdint.h>            /* Fixed-width integer types */

/*------------ MACROS : single-bit helpers ------------------------------*/
#define SET_BIT(reg,bit)    ((reg)|=(1<<(bit)))    /* Set one bit HIGH */
#define CLEAR_BIT(reg,bit)  ((reg)&=~(1<<(bit)))   /* Clear one bit LOW */
#define CHECK_BIT(reg,bit)  ((reg)&(1<<(bit)))     /* Test one bit */

/* Highest valid EEPROM address on the ATmega2560 (4KB -> 0..4095). */
#define EEPROM_SIZE_BYTES   4096U      /* Total size, used for bounds checking */
#define EEPROM_LAST_ADDR    4095U      /* Last writable address */

/*------------------------------------------------------------------------
 * ENUM : programming mode written to EEPM1:EEPM0 in EECR.
 * "Atomic" = erase+write in one operation (simplest, the default).
 * Split modes are advanced optimisations and rarely needed.
 *----------------------------------------------------------------------*/
typedef enum {
	EEPROM_MODE_ATOMIC = 0,    /* 00 : erase and write in a single ~3.3 ms operation */
	EEPROM_MODE_ERASE_ONLY,    /* 01 : erase only (~1.8 ms) */
	EEPROM_MODE_WRITE_ONLY     /* 10 : write only, assumes the cell is already erased (~1.8 ms) */
}EEPROM_MODE;

/*------------------------------------------------------------------------
 * ENUM : status returned by the write/read helpers.
 *----------------------------------------------------------------------*/
typedef enum {
	EEPROM_OK = 0,             /* Operation succeeded */
	EEPROM_ERR_RANGE,          /* Address (or address+length) was out of bounds */
	EEPROM_ERR_NULL            /* A NULL data pointer was supplied to a block call */
}EEPROM_Status;

/*------------------------------------------------------------------------
 * STRUCT : driver configuration.
 *----------------------------------------------------------------------*/
typedef struct {
	EEPROM_MODE m_mode;        /* Programming mode (atomic is the safe default) */
}EEPROM_CONFIG;

/*================================ APIs ==================================*/

/* Initialise the driver. Pass NULL to use DEFAULT_CONFIG (atomic mode). */
void EEPROM_init(const EEPROM_CONFIG* config);

/* Write one byte at 'address'. Blocks until the cell is programmed. */
EEPROM_Status EEPROM_writeByte(uint16_t address, uint8_t data);

/* Read one byte from 'address' into *data. */
EEPROM_Status EEPROM_readByte(uint16_t address, uint8_t* data);

/* Write a byte only if it differs from what is already stored (saves cycles). */
EEPROM_Status EEPROM_update(uint16_t address, uint8_t data);

/* Write a block of 'len' bytes starting at 'address'. */
EEPROM_Status EEPROM_writeBlock(uint16_t address, const uint8_t* data, uint16_t len);

/* Read a block of 'len' bytes starting at 'address'. */
EEPROM_Status EEPROM_readBlock(uint16_t address, uint8_t* data, uint16_t len);

#endif /* EEPROM_DRIVER_H_ */
