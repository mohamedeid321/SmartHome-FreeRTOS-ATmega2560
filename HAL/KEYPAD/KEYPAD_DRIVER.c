/*
 * KEYPAD_DRIVER.c
 *
 * Layer  : HAL
 * Target : ATmega2560 @ 16MHz
 * Device : 4x4 matrix keypad, non-blocking scan via MCAL/DIO.
 *
 * Author : (Mohamed Eid)
 */

#include "KEYPAD_DRIVER.h"    /* Public types, enums and prototypes */
#include <util/delay.h>       /* Tiny settle delay between driving a row and reading columns */

/*------------------------------------------------------------------------
 * The character printed by each key, laid out as [row][col].
 * Edit this map if your physical keypad has a different legend.
 *----------------------------------------------------------------------*/
static const uint8_t KEY_MAP[KEYPAD_ROWS][KEYPAD_COLS] = {
	{ '1', '2', '3', 'A' },   /* Row 0 */
	{ '4', '5', '6', 'B' },   /* Row 1 */
	{ '7', '8', '9', 'C' },   /* Row 2 */
	{ '*', '0', '#', 'D' }    /* Row 3 */
};

/*------------------------------------------------------------------------
 * Saved configuration so the scan knows which pins to use.
 *----------------------------------------------------------------------*/
static KEYPAD_Config g_kp;     /* Private copy of the active pin configuration */

/*------------------------------------------------------------------------
 * Release-debounce flag: while a key is held, we keep this set so the same
 * press is not reported again and again. It clears once the key is released.
 *----------------------------------------------------------------------*/
static uint8_t g_key_held = 0;   /* 0 = ready for a new key, 1 = a key is still down */

/*------------------------------------------------------------------------
 * DEFAULT configuration used when KEYPAD_init() receives NULL.
 * Rows on PA0..PA3 (outputs), columns on PA4..PA7 (inputs w/ pull-up).
 * (Pick a free port on your board; PORTA is just a sensible default here.)
 *----------------------------------------------------------------------*/
const KEYPAD_Config KEYPAD_DEFAULT_CONFIG = {
	.m_row_port = { DIO_PORTA, DIO_PORTA, DIO_PORTA, DIO_PORTA },   /* Rows all on PORTA */
	.m_row_pin  = { DIO_PIN0,  DIO_PIN1,  DIO_PIN2,  DIO_PIN3  },   /* R0..R3 = PA0..PA3 */
	.m_col_port = { DIO_PORTA, DIO_PORTA, DIO_PORTA, DIO_PORTA },   /* Columns all on PORTA */
	.m_col_pin  = { DIO_PIN4,  DIO_PIN5,  DIO_PIN6,  DIO_PIN7  }    /* C0..C3 = PA4..PA7 */
};

/*========================================================================
 * KEYPAD_init : configure rows as outputs (idle HIGH) and columns as
 *               inputs with pull-ups (idle HIGH).
 *======================================================================*/
void KEYPAD_init(const KEYPAD_Config* config)
{
	if(config == 0)              /* NULL-safe: use the default wiring */
	{
		config = &KEYPAD_DEFAULT_CONFIG;
	}
	g_kp = *config;              /* Keep a private copy of the configuration */

	/* Rows: outputs, parked HIGH (inactive). We pull one LOW at a time when scanning. */
	for(uint8_t r = 0; r < KEYPAD_ROWS; r++)
	{
		DIO_setDirection(g_kp.m_row_port[r], g_kp.m_row_pin[r], DIO_OUTPUT);   /* Row pin = output */
		DIO_writePin(g_kp.m_row_port[r], g_kp.m_row_pin[r], DIO_HIGH);         /* Park it HIGH (inactive) */
	}

	/* Columns: inputs with internal pull-ups, so they read HIGH until a key pulls them down. */
	for(uint8_t c = 0; c < KEYPAD_COLS; c++)
	{
		DIO_setDirection(g_kp.m_col_port[c], g_kp.m_col_pin[c], DIO_INPUT_PULLUP);   /* Column pin = input + pull-up */
	}

	g_key_held = 0;              /* Start ready to accept the first key */
}

/*========================================================================
 * KEYPAD_getKey : one non-blocking scan. Returns a key char or KEYPAD_NO_KEY.
 *======================================================================*/
uint8_t KEYPAD_getKey(void)
{
	uint8_t found = KEYPAD_NO_KEY;   /* Assume nothing is pressed until we find a key */

	/* Walk the rows one at a time. */
	for(uint8_t r = 0; r < KEYPAD_ROWS; r++)
	{
		/* Activate exactly this row by pulling it LOW (the others stay HIGH). */
		DIO_writePin(g_kp.m_row_port[r], g_kp.m_row_pin[r], DIO_LOW);
		_delay_us(5);                /* Let the line settle before reading the columns */

		/* Read each column: a LOW column means the key at (r,c) connects this row to it. */
		for(uint8_t c = 0; c < KEYPAD_COLS; c++)
		{
			if(DIO_readPin(g_kp.m_col_port[c], g_kp.m_col_pin[c]) == DIO_LOW)   /* Column pulled LOW? */
			{
				found = KEY_MAP[r][c];   /* Yes: this is the pressed key's character */
			}
		}

		/* Deactivate this row again before moving to the next one. */
		DIO_writePin(g_kp.m_row_port[r], g_kp.m_row_pin[r], DIO_HIGH);
	}

	/* ---- Release-based debounce / anti-repeat logic ---- */
	if(found != KEYPAD_NO_KEY)       /* Something is being pressed this scan */
	{
		if(g_key_held == 0)          /* ... and the previous key was already released */
		{
			g_key_held = 1;          /* Mark a key as now held (so we report it only once) */
			return found;            /* Report this fresh press to the caller */
		}
		return KEYPAD_NO_KEY;        /* Still the same held key: do not report again */
	}
	else                             /* Nothing pressed this scan */
	{
		g_key_held = 0;              /* Key released: ready to accept the next press */
		return KEYPAD_NO_KEY;        /* Report "no key" */
	}
}
