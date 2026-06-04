/*
 * App_Control.c
 *
 * Layer  : APP
 * Target : ATmega2560
 * Purpose: AUTO-mode decision rules (pure functions).
 *
 * Author : (Mohamed Eid)
 */

#include "App_Control.h"     /* Our own prototypes */
#include "SystemConfig.h"    /* Thresholds: CFG_TEMP_*, CFG_LIGHT_*, CFG_FAN_* */

/*========================================================================
 * App_Control_decideFan : temperature band -> fan state.
 *======================================================================*/
FanState App_Control_decideFan(uint16_t temperature_c)
{
	if(temperature_c < CFG_TEMP_FAN_LOW)         /* Below 25 C ... */
	{
		return FAN_OFF;                          /* ... fan off */
	}
	else if(temperature_c <= CFG_TEMP_FAN_HIGH)  /* 25..35 C ... */
	{
		return FAN_MED;                          /* ... medium speed */
	}
	else                                         /* Above 35 C ... */
	{
		return FAN_HIGH;                         /* ... full speed */
	}
}

/*========================================================================
 * App_Control_decideLight : darkness -> lamp on/off.
 *======================================================================*/
uint8_t App_Control_decideLight(uint8_t light_percent)
{
	if(light_percent < CFG_LIGHT_ON_PERCENT)     /* Darker than the threshold? */
	{
		return 1;                                /* Turn the lamp ON */
	}
	return 0;                                    /* Bright enough: lamp OFF */
}

/*========================================================================
 * App_Control_isOverTemp : alarm condition check.
 *======================================================================*/
uint8_t App_Control_isOverTemp(uint16_t temperature_c)
{
	return (temperature_c >= CFG_TEMP_ALARM) ? 1 : 0;   /* >=45 C -> alarm */
}

/*========================================================================
 * App_Control_fanSpeedPercent : fan state -> PWM percentage.
 *======================================================================*/
uint8_t App_Control_fanSpeedPercent(FanState fan)
{
	switch(fan)
	{
		case FAN_MED:  return CFG_FAN_SPEED_MED;    /* 50% */
		case FAN_HIGH: return CFG_FAN_SPEED_HIGH;   /* 100% */
		case FAN_OFF:
		default:       return 0;                    /* 0% */
	}
}
