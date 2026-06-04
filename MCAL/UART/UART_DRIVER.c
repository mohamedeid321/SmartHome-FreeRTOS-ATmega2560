/*
 * UART_DRIVER.c
 *
 * Layer  : MCAL
 * Target : ATmega2560 @ 16MHz
 * Purpose: USART driver implementation for all four USART units.
 *
 * Strategy: the four USARTs have identical register layouts, only at
 *   different addresses. We keep small pointer tables indexed by UART_UNIT
 *   so one code path serves all four. Bit positions (RXEN0, TXEN0, UDRE0,
 *   RXC0, UCSZ00 ...) are numerically the same across units, so we reuse
 *   the USART0 bit names as plain bit indices.
 *
 * Author : (Mohamed Eid)
 */

#include "UART_DRIVER.h"      /* Public types, enums and prototypes */

/*------------------------------------------------------------------------
 * Register pointer tables, indexed by UART_UNIT (0..3).
 *----------------------------------------------------------------------*/
static volatile uint8_t*  const UCSRA_REG[] = { &UCSR0A, &UCSR1A, &UCSR2A, &UCSR3A };  /* Status register A per unit */
static volatile uint8_t*  const UCSRB_REG[] = { &UCSR0B, &UCSR1B, &UCSR2B, &UCSR3B };  /* Control register B per unit */
static volatile uint8_t*  const UCSRC_REG[] = { &UCSR0C, &UCSR1C, &UCSR2C, &UCSR3C };  /* Control register C per unit */
static volatile uint16_t* const UBRR_REG[]  = { &UBRR0,  &UBRR1,  &UBRR2,  &UBRR3  };  /* 16-bit baud register per unit */
static volatile uint8_t*  const UDR_REG[]   = { &UDR0,   &UDR1,   &UDR2,   &UDR3   };  /* Data register per unit */

/*------------------------------------------------------------------------
 * Per-unit RX callbacks for interrupt mode. 'volatile' (read by the ISRs).
 *----------------------------------------------------------------------*/
static void (*volatile uart_callback[4])(uint8_t) = {0,0,0,0};   /* All empty initially */

/*------------------------------------------------------------------------
 * DEFAULT configuration used when UART_init() receives NULL.
 *----------------------------------------------------------------------*/
const UART_Config UART_DEFAULT_CONFIG = {
	.m_unit      = UART_UNIT_0,                 /* Default unit USART0 */
	.m_baud      = UART_BAUD_9600,              /* Default 9600 baud */
	.m_data_bits = UART_DATA_8BIT,              /* Default 8 data bits */
	.m_parity    = UART_PARITY_NONE,            /* Default no parity */
	.m_mode      = UART_MODE_TX_RX_POLLING,     /* Default polling mode */
	.callback    = NULL                         /* No RX callback by default */
};

/*========================================================================
 * UART_init : configure one USART unit end to end.
 *======================================================================*/
void UART_init(const UART_Config* config)
{
	if(config == NULL)               /* NULL-safe: fall back to the default config */
	{
		config = &UART_DEFAULT_CONFIG;
	}

	uint8_t u = config->m_unit;      /* Cache the unit index for the pointer tables */

	*UCSRA_REG[u] = 0;               /* Reset status register A (clears flags like U2X) */
	*UCSRB_REG[u] = 0;               /* Reset control register B (disable TX/RX/ints first) */
	*UCSRC_REG[u] = 0;               /* Reset control register C */

	/* Baud rate: UBRR = F_CPU / (16 * baud) - 1. The 2560 has a 16-bit UBRR,
	 * so we can write it in one go (no high/low juggling, no URSEL). */
	uint16_t ubrr = (uint16_t)((F_CPU / (16UL * (uint32_t)config->m_baud)) - 1);
	*UBRR_REG[u] = ubrr;             /* Program the 16-bit baud register */

	/* Frame format: parity (UPM1:UPM0 = bits 5:4) + data bits (UCSZ1:0 = bits 2:1).
	 * Bit positions are identical across units, so the USART0 names work as indices. */
	*UCSRC_REG[u] = ((config->m_parity   & 0x03) << UPM00)    /* Set the 2 parity bits */
	             |  ((config->m_data_bits & 0x03) << UCSZ00); /* Set the 2 data-size bits (8-bit default) */

	/* Enable transmitter and receiver, and the RX interrupt if requested. */
	switch(config->m_mode)
	{
		case UART_MODE_TX_RX_POLLING:                 /* Polling: just TX + RX */
			SET_BIT(*UCSRB_REG[u], RXEN0);            /* Enable the receiver */
			SET_BIT(*UCSRB_REG[u], TXEN0);            /* Enable the transmitter */
			break;
		case UART_MODE_TX_RX_INTERRUPT:               /* Interrupt: TX + RX + RX-complete int */
			SET_BIT(*UCSRB_REG[u], RXEN0);            /* Enable the receiver */
			SET_BIT(*UCSRB_REG[u], TXEN0);            /* Enable the transmitter */
			SET_BIT(*UCSRB_REG[u], RXCIE0);           /* Enable the RX-complete interrupt */
			uart_callback[u] = config->callback;      /* Remember this unit's RX callback */
			sei();                                    /* Global interrupt enable */
			break;
	}
}

/*========================================================================
 * UART_sendByte : blocking transmit of one byte.
 *======================================================================*/
void UART_sendByte(UART_UNIT unit, uint8_t data)
{
	while(!CHECK_BIT(*UCSRA_REG[unit], UDRE0));   /* Wait until the data register is empty (UDRE high) */
	*UDR_REG[unit] = data;                        /* Write the byte; hardware shifts it out */
}

/*========================================================================
 * UART_readByte : blocking receive of one byte.
 *======================================================================*/
uint8_t UART_readByte(UART_UNIT unit)
{
	while(!CHECK_BIT(*UCSRA_REG[unit], RXC0));    /* Wait until a byte has been received (RXC high) */
	return *UDR_REG[unit];                        /* Read and return the received byte */
}

/*========================================================================
 * UART_available : non-blocking "is a byte waiting?" check.
 *======================================================================*/
uint8_t UART_available(UART_UNIT unit)
{
	return CHECK_BIT(*UCSRA_REG[unit], RXC0);     /* Non-zero if the RX-complete flag is set */
}

/*========================================================================
 * UART_sendString : transmit a null-terminated string.
 *======================================================================*/
void UART_sendString(UART_UNIT unit, const char* str)
{
	while(*str)                       /* Until the terminating '\0' ... */
	{
		UART_sendByte(unit, *str++);  /* ... send the current char and advance */
	}
}

/*========================================================================
 * UART_sendNumber : transmit an unsigned number as ASCII digits.
 *======================================================================*/
void UART_sendNumber(UART_UNIT unit, uint16_t num)
{
	if(num == 0)                      /* Special-case zero ... */
	{
		UART_sendByte(unit, '0');     /* ... just send '0' */
		return;
	}
	char digits[5];                   /* Up to 5 digits for a 16-bit number (max 65535) */
	int8_t i = 0;                     /* Index into the digit buffer */
	while(num > 0)                    /* Extract digits least-significant first */
	{
		digits[i++] = '0' + (num % 10);  /* Convert the last digit to its ASCII char */
		num /= 10;                       /* Drop the last digit */
	}
	while(i > 0)                      /* Send the buffered digits in reverse (correct) order */
	{
		UART_sendByte(unit, digits[--i]);
	}
}

/*========================================================================
 * RX-complete ISRs : one per unit, each forwarding to its callback.
 *======================================================================*/
ISR(USART0_RX_vect){ uint8_t d = UDR0; if(uart_callback[0]) uart_callback[0](d); }  /* USART0 byte arrived */
ISR(USART1_RX_vect){ uint8_t d = UDR1; if(uart_callback[1]) uart_callback[1](d); }  /* USART1 byte arrived */
ISR(USART2_RX_vect){ uint8_t d = UDR2; if(uart_callback[2]) uart_callback[2](d); }  /* USART2 byte arrived */
ISR(USART3_RX_vect){ uint8_t d = UDR3; if(uart_callback[3]) uart_callback[3](d); }  /* USART3 byte arrived */
