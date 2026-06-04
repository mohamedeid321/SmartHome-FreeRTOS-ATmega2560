/*
 * LCD_DRIVER.c
 *
 * Layer  : HAL
 * Target : ATmega2560 @ 16MHz
 * Device : 20x4 HD44780 LCD, 4-bit mode, driven through MCAL/DIO.
 *
 * The send/init logic follows the author's original 16x2 routine (nibble
 * transfer, 0x3,0x3,0x3,0x2 wake-up sequence) but every raw port write is
 * replaced by a DIO call and the 4-row addressing is added.
 *
 * Author : (Mohamed Eid)
 */

#include "LCD_DRIVER.h"       /* Public types, enums and prototypes */

/*------------------------------------------------------------------------
 * Saved configuration so every routine knows which pins to drive.
 *----------------------------------------------------------------------*/
static LCD_Config g_lcd;       /* A private copy of the active pin configuration */

/*------------------------------------------------------------------------
 * DEFAULT configuration used when LCD_init() receives NULL.
 * Data nibble on PORTC (PC4..PC7); RS=PA2, RW=PA1, E=PA0  (matches the
 * author's original wiring so existing Proteus sheets still line up).
 *----------------------------------------------------------------------*/
const LCD_Config LCD_DEFAULT_CONFIG = {
	.m_data_port = DIO_PORTC,    /* D4..D7 on PC4..PC7 */
	.m_rs_port   = DIO_PORTA, .m_rs_pin = DIO_PIN2,   /* RS on PA2 */
	.m_rw_port   = DIO_PORTA, .m_rw_pin = DIO_PIN1,   /* RW on PA1 */
	.m_en_port   = DIO_PORTA, .m_en_pin = DIO_PIN0    /* E  on PA0 */
};

/*------------------------------------------------------------------------
 * Helper: pulse the Enable line so the LCD latches the nibble on the bus.
 *----------------------------------------------------------------------*/
static void lcd_enable_pulse(void)
{
	DIO_writePin(g_lcd.m_en_port, g_lcd.m_en_pin, DIO_HIGH);   /* E HIGH: open the latch */
	_delay_us(1);                                              /* Hold > 450 ns (datasheet min) */
	DIO_writePin(g_lcd.m_en_port, g_lcd.m_en_pin, DIO_LOW);    /* E LOW: latch the data */
	_delay_us(100);                                            /* Let the controller process it */
}

/*------------------------------------------------------------------------
 * Helper: put the high 4 bits of 'nibble' onto data lines D4..D7, then pulse E.
 *----------------------------------------------------------------------*/
static void lcd_send_nibble(uint8_t nibble)
{
	/* Drive each of the 4 data lines (port pins 4..7) from bits 4..7 of 'nibble'. */
	DIO_writePin(g_lcd.m_data_port, DIO_PIN4, (nibble & 0x10) ? DIO_HIGH : DIO_LOW);  /* D4 = bit4 */
	DIO_writePin(g_lcd.m_data_port, DIO_PIN5, (nibble & 0x20) ? DIO_HIGH : DIO_LOW);  /* D5 = bit5 */
	DIO_writePin(g_lcd.m_data_port, DIO_PIN6, (nibble & 0x40) ? DIO_HIGH : DIO_LOW);  /* D6 = bit6 */
	DIO_writePin(g_lcd.m_data_port, DIO_PIN7, (nibble & 0x80) ? DIO_HIGH : DIO_LOW);  /* D7 = bit7 */
	lcd_enable_pulse();                                                              /* Latch this nibble */
}

/*========================================================================
 * LCD_sendCommand : send an instruction byte (RS = 0).
 *======================================================================*/
void LCD_sendCommand(uint8_t cmd)
{
	DIO_writePin(g_lcd.m_rs_port, g_lcd.m_rs_pin, DIO_LOW);    /* RS = 0 -> this byte is a command */
	lcd_send_nibble(cmd & 0xF0);                              /* Send the high nibble first */
	lcd_send_nibble(cmd << 4);                                /* Then the low nibble (shifted up) */
	_delay_ms(2);                                             /* Most commands need up to ~1.6 ms */
}

/*========================================================================
 * LCD_sendChar : send a data byte / character (RS = 1).
 *======================================================================*/
void LCD_sendChar(uint8_t ch)
{
	DIO_writePin(g_lcd.m_rs_port, g_lcd.m_rs_pin, DIO_HIGH);   /* RS = 1 -> this byte is display data */
	lcd_send_nibble(ch & 0xF0);                               /* High nibble first */
	lcd_send_nibble(ch << 4);                                 /* Low nibble */
	_delay_us(100);                                           /* A character write is fast (~43 us) */
}

/*========================================================================
 * LCD_init : configure pins and run the HD44780 4-bit wake-up sequence.
 *======================================================================*/
void LCD_init(const LCD_Config* config)
{
	if(config == 0)              /* NULL-safe: use the default wiring */
	{
		config = &LCD_DEFAULT_CONFIG;
	}
	g_lcd = *config;             /* Keep a private copy of the configuration */

	/* Make the four data lines outputs. */
	DIO_setDirection(g_lcd.m_data_port, DIO_PIN4, DIO_OUTPUT);   /* D4 output */
	DIO_setDirection(g_lcd.m_data_port, DIO_PIN5, DIO_OUTPUT);   /* D5 output */
	DIO_setDirection(g_lcd.m_data_port, DIO_PIN6, DIO_OUTPUT);   /* D6 output */
	DIO_setDirection(g_lcd.m_data_port, DIO_PIN7, DIO_OUTPUT);   /* D7 output */

	/* Make the three control lines outputs. */
	DIO_setDirection(g_lcd.m_rs_port, g_lcd.m_rs_pin, DIO_OUTPUT);   /* RS output */
	DIO_setDirection(g_lcd.m_rw_port, g_lcd.m_rw_pin, DIO_OUTPUT);   /* RW output */
	DIO_setDirection(g_lcd.m_en_port, g_lcd.m_en_pin, DIO_OUTPUT);   /* E  output */

	DIO_writePin(g_lcd.m_rw_port, g_lcd.m_rw_pin, DIO_LOW);          /* RW = 0 -> we only ever write */

	_delay_ms(40);               /* Wait > 15 ms after power-up before talking to the LCD */

	/* Wake-up sequence: 0x3, 0x3, 0x3, then 0x2 to enter 4-bit mode. */
	lcd_send_nibble(0x30); _delay_ms(5);     /* Function set (8-bit), 1st try */
	lcd_send_nibble(0x30); _delay_us(150);   /* 2nd try */
	lcd_send_nibble(0x30); _delay_us(150);   /* 3rd try */
	lcd_send_nibble(0x20); _delay_us(150);   /* Switch to 4-bit interface */

	/* Now configure the display through normal commands. */
	LCD_sendCommand(LCD_FUNCTION_4BIT_2LN);    /* 4-bit bus, 2-line font (also correct for 4-row panels) */
	LCD_sendCommand(LCD_DISPLAY_ON_CUR_OFF);   /* Display on, cursor off */
	LCD_sendCommand(LCD_CLEAR);                /* Clear the screen */
	_delay_ms(2);                              /* Clear needs extra time */
	LCD_sendCommand(LCD_ENTRY_MODE_INC);       /* Auto-increment cursor to the right */
}

/*========================================================================
 * LCD_clear : wipe the display.
 *======================================================================*/
void LCD_clear(void)
{
	LCD_sendCommand(LCD_CLEAR);   /* Issue the clear instruction */
	_delay_ms(2);                 /* Clear/home take up to ~1.6 ms */
}

/*========================================================================
 * LCD_gotoXY : move the cursor to (row, col) on a 20x4 panel.
 *======================================================================*/
void LCD_gotoXY(uint8_t row, uint8_t col)
{
	uint8_t base;                 /* DDRAM base address of the requested row */
	switch(row)
	{
		case 0:  base = LCD_ROW0_ADDR; break;   /* Row 0 -> 0x00 */
		case 1:  base = LCD_ROW1_ADDR; break;   /* Row 1 -> 0x40 */
		case 2:  base = LCD_ROW2_ADDR; break;   /* Row 2 -> 0x14 (NOT contiguous!) */
		case 3:  base = LCD_ROW3_ADDR; break;   /* Row 3 -> 0x54 */
		default: base = LCD_ROW0_ADDR; break;   /* Out-of-range row falls back to row 0 */
	}
	LCD_sendCommand(LCD_SET_DDRAM | (base + col));   /* Set DDRAM address = base + column */
}

/*========================================================================
 * LCD_printString : write a C string from the cursor onward.
 *======================================================================*/
void LCD_printString(const char* str)
{
	while(*str)                   /* Until the terminating '\0' ... */
	{
		LCD_sendChar(*str++);     /* ... print the current char and advance */
	}
}

/*========================================================================
 * LCD_printNumber : write an unsigned integer in decimal.
 *======================================================================*/
void LCD_printNumber(uint16_t num)
{
	char buf[6];                  /* Up to 5 digits (65535) + '\0' */
	int8_t i = 0;                 /* Buffer index */

	if(num == 0)                  /* Special-case zero ... */
	{
		LCD_sendChar('0');        /* ... print a single '0' */
		return;
	}
	while(num > 0)                /* Extract digits least-significant first */
	{
		buf[i++] = '0' + (num % 10);   /* Convert last digit to ASCII */
		num /= 10;                     /* Drop it */
	}
	while(i > 0)                  /* Print the digits back in correct order */
	{
		LCD_sendChar(buf[--i]);
	}
}
