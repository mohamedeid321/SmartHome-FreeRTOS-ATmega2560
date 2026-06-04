/*
 * App_Fault.c
 *
 * Layer  : APP
 * Target : ATmega2560
 * Purpose: Fault-detection rules (pure functions).
 *
 * Author : (Mohamed Eid)
 */

#include "App_Fault.h"       /* Our own prototypes */
#include "SystemConfig.h"    /* CFG_TEMP_ALARM */

/*========================================================================
 * App_Fault_checkTemp : classify a temperature reading.
 *======================================================================*/
FaultCode App_Fault_checkTemp(uint16_t temperature_c)
{
	/* Impossible reading first: a disconnected or shorted LM35 reads far
	 * outside any real room temperature. Treat that as a SENSOR fault, not
	 * an over-temperature alarm. */
	if(temperature_c < FAULT_TEMP_MIN_C || temperature_c > FAULT_TEMP_MAX_C)
	{
		return FAULT_TEMP_SENSOR;        /* Wiring / hardware fault */
	}

	/* Valid reading, but is it dangerously hot? */
	if(temperature_c >= CFG_TEMP_ALARM)
	{
		return FAULT_OVER_TEMP;          /* Genuine over-temperature */
	}

	return FAULT_NONE;                   /* Reading is fine */
}

/*========================================================================
 * App_Fault_checkLight : validate a light percentage.
 *======================================================================*/
FaultCode App_Fault_checkLight(uint8_t light_percent)
{
	if(light_percent > 100)              /* A percentage can never exceed 100 ... */
	{
		return FAULT_LIGHT_SENSOR;       /* ... so the reading/wiring is wrong */
	}
	return FAULT_NONE;                   /* Valid */
}

/*========================================================================
 * App_Fault_toString : short label for display / terminal.
 *======================================================================*/
const char* App_Fault_toString(FaultCode code)
{
	switch(code)
	{
		case FAULT_NONE:        return "OK";
		case FAULT_TEMP_SENSOR: return "TEMP SENSOR FAULT";
		case FAULT_LIGHT_SENSOR:return "LIGHT SENSOR FAULT";
		case FAULT_SD_CARD:     return "SD CARD FAULT";
		case FAULT_OVER_TEMP:   return "OVER TEMPERATURE";
		default:                return "UNKNOWN";
	}
}
