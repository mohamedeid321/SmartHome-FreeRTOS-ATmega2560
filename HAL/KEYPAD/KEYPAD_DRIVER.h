/*
 * KEYPAD_DRIVER.h
 *
 * Layer  : HAL (Hardware Abstraction Layer)
 * Target : ATmega2560 @ 16MHz
 * Device : 4x4 matrix keypad (16 keys, 8 wires: 4 rows + 4 columns).
 * Depends: MCAL/DIO.
 *
 * Method : NON-BLOCKING column-scan with debounce.
 *   - Rows are driven as outputs, one pulled LOW at a time.
 *   - Columns are inputs with internal pull-ups (idle HIGH).
 *   - A pressed key ties its row to its column, so when we pull a row LOW
 *     and read a column LOW, that (row,col) intersection is the key.
 *   - KEYPAD_getKey() scans once and returns immediately (no waiting), so
 *     it can be called periodically from a FreeRTOS task.
 *   - A simple state flag prevents auto-repeat: a new key is only reported
 *     after the previous key has been released.
 *
 * Author : (Mohamed Eid)
 */

#ifndef KEYPAD_DRIVER_H_       /* Include guard opening */
#define KEYPAD_DRIVER_H_       /* Guard token */

#include <stdint.h>            /* Fixed-width integer types */
#include "DIO_DRIVER.h"        /* Sits on top of the MCAL DIO driver */

/*------------------------------------------------------------------------
 * Geometry. A 4x4 keypad. Change these if you wire a different matrix.
 *----------------------------------------------------------------------*/
#define KEYPAD_ROWS   4        /* Number of rows */
#define KEYPAD_COLS   4        /* Number of columns */

/* Value returned when no key is currently pressed. */
#define KEYPAD_NO_KEY   0      /* 0 means "nothing pressed this scan" */

/*------------------------------------------------------------------------
 * STRUCT : the four row pins and four column pins, each as (port, pin).
 * Rows are outputs; columns are inputs with pull-ups (set up in init).
 *----------------------------------------------------------------------*/
typedef struct {
	DIO_PORT m_row_port[KEYPAD_ROWS];   /* Port of each row pin */
	DIO_PIN  m_row_pin [KEYPAD_ROWS];   /* Pin index of each row */
	DIO_PORT m_col_port[KEYPAD_COLS];   /* Port of each column pin */
	DIO_PIN  m_col_pin [KEYPAD_COLS];   /* Pin index of each column */
}KEYPAD_Config;

/*================================ APIs ==================================*/

/* Initialise the keypad pins. Pass NULL to use KEYPAD_DEFAULT_CONFIG. */
void KEYPAD_init(const KEYPAD_Config* config);

/* Non-blocking scan. Returns the pressed character (e.g. '7', '*', 'A'),
 * or KEYPAD_NO_KEY if nothing new is pressed. Call this repeatedly. */
uint8_t KEYPAD_getKey(void);

/* Ready-made default config (see the .c for the exact pin choices). */
extern const KEYPAD_Config KEYPAD_DEFAULT_CONFIG;

#endif /* KEYPAD_DRIVER_H_ */
