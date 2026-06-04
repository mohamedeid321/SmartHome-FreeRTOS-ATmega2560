/*
 * MOTOR_DRIVER.c
 *
 * Layer  : HAL
 * Target : ATmega2560 @ 16MHz
 * Device : DC motor via H-bridge; direction on DIO, speed on a Fast-PWM timer.
 *
 * Speed mapping:
 *   The timer runs Fast-PWM with an 8-bit compare value OCR (0..255), where
 *   duty = OCR/255. We map a 0..100% request onto 0..255 for OCR.
 *
 * Author : (Mohamed Eid)
 */

#include "MOTOR_DRIVER.h"    /* Public types, enums and prototypes */

/*------------------------------------------------------------------------
 * Saved configuration.
 *----------------------------------------------------------------------*/
static MOTOR_Config g_motor;   /* Private copy of the active configuration */

/*------------------------------------------------------------------------
 * DEFAULT configuration used when MOTOR_init() receives NULL.
 * IN1=PA6, IN2=PA7, PWM on Timer0 (ENABLE must be wired to OC0A = PB7).
 *----------------------------------------------------------------------*/
const MOTOR_Config MOTOR_DEFAULT_CONFIG = {
	.m_in1_port  = DIO_PORTA, .m_in1_pin = DIO_PIN6,   /* IN1 on PA6 */
	.m_in2_port  = DIO_PORTA, .m_in2_pin = DIO_PIN7,   /* IN2 on PA7 */
	.m_pwm_timer = Timer0                               /* Speed via Timer0 (OC0A = PB7) */
};

/*========================================================================
 * MOTOR_init : set up direction pins and the PWM timer; start stopped.
 *======================================================================*/
void MOTOR_init(const MOTOR_Config* config)
{
	if(config == 0)              /* NULL-safe: use the default config */
	{
		config = &MOTOR_DEFAULT_CONFIG;
	}
	g_motor = *config;           /* Keep a private copy of the configuration */

	/* Direction pins are outputs. */
	DIO_setDirection(g_motor.m_in1_port, g_motor.m_in1_pin, DIO_OUTPUT);   /* IN1 output */
	DIO_setDirection(g_motor.m_in2_port, g_motor.m_in2_pin, DIO_OUTPUT);   /* IN2 output */

	/* Configure the timer in Fast-PWM so its OC pin carries the speed signal. */
	TIMER_CONFIG_SELCECTION tcfg;
	tcfg.m_timer_id        = g_motor.m_pwm_timer;     /* Chosen PWM timer */
	tcfg.m_timer_mode      = TIMER_MODE_FAST_PWM;     /* Fast PWM drives the OC pin directly */
	tcfg.m_timer_prescaler = TIMER_PRESCALER_8;       /* /8 -> a few kHz PWM, smooth for a motor */
	tcfg.m_ocr_value       = 0;                       /* Start at 0% duty (stopped) */
	tcfg.callback          = 0;                       /* No ISR; PWM is hardware-generated */
	TIMER_setup(&tcfg);                               /* This also makes the OC pin an output */

	MOTOR_stop();                /* Begin in a safe, stopped state */
}

/*========================================================================
 * MOTOR_setDirection : program the two H-bridge inputs.
 *======================================================================*/
void MOTOR_setDirection(MOTOR_Direction dir)
{
	switch(dir)
	{
		case MOTOR_FORWARD:                                              /* IN1 HIGH, IN2 LOW */
			DIO_writePin(g_motor.m_in1_port, g_motor.m_in1_pin, DIO_HIGH);
			DIO_writePin(g_motor.m_in2_port, g_motor.m_in2_pin, DIO_LOW);
			break;
		case MOTOR_BACKWARD:                                             /* IN1 LOW, IN2 HIGH */
			DIO_writePin(g_motor.m_in1_port, g_motor.m_in1_pin, DIO_LOW);
			DIO_writePin(g_motor.m_in2_port, g_motor.m_in2_pin, DIO_HIGH);
			break;
		case MOTOR_STOP:                                                 /* Both equal -> brake/stop */
		default:
			DIO_writePin(g_motor.m_in1_port, g_motor.m_in1_pin, DIO_LOW);
			DIO_writePin(g_motor.m_in2_port, g_motor.m_in2_pin, DIO_LOW);
			break;
	}
}

/*========================================================================
 * MOTOR_setSpeed : map 0..100% to the 0..255 PWM compare value.
 *======================================================================*/
void MOTOR_setSpeed(uint8_t percent)
{
	if(percent > 100) percent = 100;                          /* Clamp to a valid percentage */

	uint8_t ocr = (uint8_t)(((uint16_t)percent * 255U) / 100U);   /* Scale % -> 0..255 duty */
	TIMER_setOCR(g_motor.m_pwm_timer, ocr);                   /* Apply the new duty cycle */
}

/*========================================================================
 * MOTOR_drive : set direction and speed together.
 *======================================================================*/
void MOTOR_drive(MOTOR_Direction dir, uint8_t percent)
{
	MOTOR_setDirection(dir);     /* Choose the direction first ... */
	MOTOR_setSpeed(percent);     /* ... then apply the speed */
}

/*========================================================================
 * MOTOR_stop : stop direction and force 0% speed.
 *======================================================================*/
void MOTOR_stop(void)
{
	MOTOR_setDirection(MOTOR_STOP);   /* Both inputs LOW */
	MOTOR_setSpeed(0);                /* 0% duty -> no drive */
}
