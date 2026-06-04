/*
 * UART_DRIVER.h
 *
 * Layer  : MCAL
 * Target : ATmega2560 @ 16MHz
 * Purpose: USART driver, adapted from the author's ATmega32 code. The 2560
 *          has FOUR independent USARTs (USART0..USART3) instead of one, so
 *          this driver adds a "unit" selector. Pins:
 *            USART0: RXD0=PE0  TXD0=PE1   (also the USB / programmer port)
 *            USART1: RXD1=PD2  TXD1=PD3
 *            USART2: RXD2=PH0  TXD2=PH1
 *            USART3: RXD3=PJ0  TXD3=PJ1
 *
 * 2560 vs ATmega32 register differences handled here:
 *   - Registers are numbered: UCSR0A/B/C, UBRR0H/L, UDR0, ... per unit.
 *   - There is NO URSEL bit; UBRRH and UCSRC are separate registers, so the
 *     ATmega32 URSEL juggling is removed.
 *
 * Author : (Mohamed Eid)
 */

#ifndef UART_DRIVER_H_         /* Include guard opening */
#define UART_DRIVER_H_         /* Guard token */

#include <avr/io.h>            /* AVR register map (UCSRnA/B/C, UBRRn, UDRn ...) */
#include <avr/interrupt.h>     /* ISR() macro for RX interrupt mode */
#include <stdint.h>            /* Fixed-width integer types */

#ifndef F_CPU                  /* Clock needed for the baud-rate maths */
#define F_CPU 16000000UL       /* Default to 16 MHz if the build system did not define it */
#endif

#ifndef NULL
#define NULL 0                 /* Provide NULL if not already defined */
#endif

/*------------ MACROS : single-bit helpers ------------------------------*/
#define SET_BIT(reg,bit)    ((reg)|=(1<<(bit)))    /* Set one bit HIGH */
#define CLEAR_BIT(reg,bit)  ((reg)&=~(1<<(bit)))   /* Clear one bit LOW */
#define CHECK_BIT(reg,bit)  ((reg)&(1<<(bit)))     /* Test one bit */

/*------------------------------------------------------------------------
 * ENUM : which physical USART unit to drive.
 *----------------------------------------------------------------------*/
typedef enum {
	UART_UNIT_0 = 0,   /* USART0 (PE0/PE1) */
	UART_UNIT_1,       /* USART1 (PD2/PD3) */
	UART_UNIT_2,       /* USART2 (PH0/PH1) */
	UART_UNIT_3        /* USART3 (PJ0/PJ1) */
}UART_UNIT;

/*------------------------------------------------------------------------
 * ENUM : common baud rates (the numeric value IS the baud).
 *----------------------------------------------------------------------*/
typedef enum {
	UART_BAUD_2400   = 2400,
	UART_BAUD_4800   = 4800,
	UART_BAUD_9600   = 9600,
	UART_BAUD_19200  = 19200,
	UART_BAUD_38400  = 38400,
	UART_BAUD_57600  = 57600,
	UART_BAUD_115200 = 115200
}UART_BAUDRATE;

/*------------------------------------------------------------------------
 * ENUM : number of data bits (UCSZ1:UCSZ0).
 *----------------------------------------------------------------------*/
typedef enum {
	UART_DATA_5BIT = 0,
	UART_DATA_6BIT = 1,
	UART_DATA_7BIT = 2,
	UART_DATA_8BIT = 3    /* Most common */
}UART_DataBits;

/*------------------------------------------------------------------------
 * ENUM : parity mode (UPM1:UPM0).
 *----------------------------------------------------------------------*/
typedef enum {
	UART_PARITY_NONE = 0,   /* Most common */
	UART_PARITY_EVEN = 2,
	UART_PARITY_ODD  = 3
}UART_Parity;

/*------------------------------------------------------------------------
 * ENUM : driving style.
 *----------------------------------------------------------------------*/
typedef enum {
	UART_MODE_TX_RX_POLLING,    /* Blocking send/receive */
	UART_MODE_TX_RX_INTERRUPT   /* RX delivered via callback */
}UART_Mode;

/*------------------------------------------------------------------------
 * STRUCT : full configuration of one USART unit.
 *----------------------------------------------------------------------*/
typedef struct {
	UART_UNIT      m_unit;        /* Which USART (0..3) */
	UART_BAUDRATE  m_baud;        /* Baud rate */
	UART_DataBits  m_data_bits;   /* Data bits */
	UART_Parity    m_parity;      /* Parity */
	UART_Mode      m_mode;        /* Polling or interrupt */
	void (*callback)(uint8_t);    /* RX callback for interrupt mode (NULL = none) */
}UART_Config;

/*================================ APIs ==================================*/

/* Initialise one USART unit. Pass NULL to use UART_DEFAULT_CONFIG. */
void UART_init(const UART_Config* config);

/* Send one byte (blocking) on a given unit. */
void UART_sendByte(UART_UNIT unit, uint8_t data);

/* Send a number as ASCII digits on a given unit. */
void UART_sendNumber(UART_UNIT unit, uint16_t num);

/* Send a null-terminated string on a given unit. */
void UART_sendString(UART_UNIT unit, const char* str);

/* Blocking read of one byte from a given unit. */
uint8_t UART_readByte(UART_UNIT unit);

/* Non-blocking check: returns non-zero if a byte is waiting on a unit. */
uint8_t UART_available(UART_UNIT unit);

/* Ready-made default config (USART0, 9600, 8N1, polling). */
extern const UART_Config UART_DEFAULT_CONFIG;

#endif /* UART_DRIVER_H_ */
