/*
 * MOTOR_DRIVER.h
 *
 * Layer  : HAL (Hardware Abstraction Layer)
 * Target : ATmega2560 @ 16MHz
 * Device : DC motor via an H-bridge (e.g. L298N).
 * Depends: MCAL/DIO (direction) and MCAL/TIMER (PWM speed).
 *
 * Control model:
 *   - Direction comes from two H-bridge inputs IN1/IN2:
 *       IN1=HIGH IN2=LOW  -> forward
 *       IN1=LOW  IN2=HIGH -> backward
 *       IN1==IN2          -> stop / brake
 *   - Speed comes from a PWM signal on the H-bridge ENABLE pin. We use a
 *     timer in Fast-PWM; the duty cycle (0..100%) sets the speed. The motor
 *     ENABLE pin MUST be wired to that timer's OC pin:
 *       Timer0 -> OC0A = PB7
 *       Timer2 -> OC2A = PB4
 *
 * Author : (Mohamed Eid)
 */

#ifndef MOTOR_DRIVER_H_        /* Include guard opening */
#define MOTOR_DRIVER_H_        /* Guard token */

#include <stdint.h>            /* Fixed-width integer types */
#include "DIO_DRIVER.h"        /* Direction pins via DIO */
#include "TIMER_DRIVER.h"      /* Speed via timer PWM */

/*------------------------------------------------------------------------
 * ENUM : motor direction / motion command.
 *----------------------------------------------------------------------*/
typedef enum {
	MOTOR_STOP = 0,   /* Both inputs equal -> no motion */
	MOTOR_FORWARD,    /* IN1 HIGH, IN2 LOW */
	MOTOR_BACKWARD    /* IN1 LOW,  IN2 HIGH */
}MOTOR_Direction;

/*------------------------------------------------------------------------
 * STRUCT : motor configuration.
 *   IN1/IN2 are plain GPIO (direction).
 *   m_pwm_timer drives the ENABLE pin (speed) - see header notes for the pin.
 *----------------------------------------------------------------------*/
typedef struct {
	DIO_PORT        m_in1_port;    /* Port of H-bridge input 1 */
	DIO_PIN         m_in1_pin;     /* Pin of input 1 */
	DIO_PORT        m_in2_port;    /* Port of H-bridge input 2 */
	DIO_PIN         m_in2_pin;     /* Pin of input 2 */
	TIMER_SELECT_ID m_pwm_timer;   /* Timer driving the ENABLE (speed) pin */
}MOTOR_Config;

/*================================ APIs ==================================*/

/* Initialise direction pins + PWM timer; motor starts stopped at 0% speed.
 * Pass NULL to use MOTOR_DEFAULT_CONFIG. */
void MOTOR_init(const MOTOR_Config* config);

/* Set the direction (forward / backward / stop). */
void MOTOR_setDirection(MOTOR_Direction dir);

/* Set the speed as a percentage 0..100 (0 = stop, 100 = full). */
void MOTOR_setSpeed(uint8_t percent);

/* Convenience: set direction and speed in one call. */
void MOTOR_drive(MOTOR_Direction dir, uint8_t percent);

/* Stop the motor (direction stop + 0% speed). */
void MOTOR_stop(void);

/* Ready-made default config (IN1=PA6, IN2=PA7, PWM on Timer0/OC0A=PB7). */
extern const MOTOR_Config MOTOR_DEFAULT_CONFIG;

#endif /* MOTOR_DRIVER_H_ */
