/*
 * LCD_DRIVER.h
 *
 * Layer  : HAL (Hardware Abstraction Layer)
 * Target : ATmega2560 @ 16MHz
 * Device : 20x4 character LCD (HD44780 controller) in 4-bit mode.
 * Depends: MCAL/DIO  (this driver never touches registers directly).
 *
 * What changed vs the original 16x2 driver:
 *   1) Added the two extra line base addresses (0x14, 0x54) so all 4 rows
 *      of a 20x4 display are addressable. (The controller's row addresses
 *      are NOT contiguous - this is the classic 20x4 gotcha.)
 *   2) Pins are now chosen through an LCD_Config struct (with a default
 *      config), so the wiring can change without editing the driver.
 *   3) All low-level pin access goes through the DIO driver.
 *
 * Wiring model: 4-bit mode. Data lines D4..D7 occupy the top nibble of a
 * chosen data port. RS / RW / E are three individually chosen pins.
 *
 * Author : (Mohamed Eid)
 */

#ifndef LCD_DRIVER_H_          /* Include guard opening */
#define LCD_DRIVER_H_          /* Guard token */

#ifndef F_CPU                  /* Needed by util/delay for the timing loops */
#define F_CPU 16000000UL       /* 16 MHz board clock */
#endif

#include <util/delay.h>        /* _delay_ms / _delay_us for HD44780 timing */
#include <stdint.h>            /* Fixed-width integer types */
#include "DIO_DRIVER.h"        /* Sits on top of the MCAL DIO driver */

/*------------------------------------------------------------------------
 * Geometry of the panel. Change these two lines for a different size.
 *----------------------------------------------------------------------*/
#define LCD_ROWS   4           /* Number of character rows */
#define LCD_COLS   20          /* Number of characters per row */

/*------------------------------------------------------------------------
 * HD44780 instruction set (the subset we use).
 *----------------------------------------------------------------------*/
#define LCD_CLEAR              0x01   /* Clear display, cursor home */
#define LCD_ENTRY_MODE_INC     0x06   /* Move cursor right after each write */
#define LCD_DISPLAY_ON_CUR_OFF 0x0C   /* Display ON, cursor + blink OFF */
#define LCD_FUNCTION_4BIT_2LN  0x28   /* 4-bit bus, 2-line font (used for 2 AND 4 row panels) */
#define LCD_SET_DDRAM          0x80   /* OR with an address to move the cursor */

/*------------------------------------------------------------------------
 * Row base addresses in DDRAM. NOTE the non-obvious values for a 20x4:
 * row2 starts at 0x14 and row3 at 0x54 (row2/3 are continuations of 0/1).
 *----------------------------------------------------------------------*/
#define LCD_ROW0_ADDR   0x00
#define LCD_ROW1_ADDR   0x40
#define LCD_ROW2_ADDR   0x14
#define LCD_ROW3_ADDR   0x54

/*------------------------------------------------------------------------
 * STRUCT : which pins the LCD is wired to. Data uses the TOP nibble
 * (D4..D7) of m_data_port. Control pins are chosen individually.
 *----------------------------------------------------------------------*/
typedef struct {
	DIO_PORT m_data_port;   /* Port whose upper nibble (pins 4..7) carries D4..D7 */
	DIO_PORT m_rs_port;     /* Port of the RS (register select) pin */
	DIO_PIN  m_rs_pin;      /* RS pin index */
	DIO_PORT m_rw_port;     /* Port of the RW (read/write) pin */
	DIO_PIN  m_rw_pin;      /* RW pin index (tie to GND in HW if you prefer) */
	DIO_PORT m_en_port;     /* Port of the E (enable) pin */
	DIO_PIN  m_en_pin;      /* E pin index */
}LCD_Config;

/*================================ APIs ==================================*/

/* Initialise the LCD with a pin configuration. Pass NULL for LCD_DEFAULT_CONFIG. */
void LCD_init(const LCD_Config* config);

/* Send a command byte (e.g. clear, cursor move). */
void LCD_sendCommand(uint8_t cmd);

/* Send a single character to the current cursor position. */
void LCD_sendChar(uint8_t ch);

/* Clear the whole display and return the cursor home. */
void LCD_clear(void);

/* Move the cursor to (row, col). row 0..3, col 0..19. */
void LCD_gotoXY(uint8_t row, uint8_t col);

/* Print a null-terminated string from the current cursor position. */
void LCD_printString(const char* str);

/* Print an unsigned integer as decimal digits. */
void LCD_printNumber(uint16_t num);

/* Ready-made default config (see the .c for the exact pin choices). */
extern const LCD_Config LCD_DEFAULT_CONFIG;

#endif /* LCD_DRIVER_H_ */
