/*
 * resource_pwm_motor.c
 *
 *  Created on: Dec 24, 2019
 *      Author: byoungnamjang
 */

#include <tizen.h>
#include <peripheral_io.h>
#include <unistd.h>
#include "hello.h"

#define ANCHOR3_PWM_CHIPID	0
#define ANCHOR3_PWM_PIN		0
#define MILLI_SECOND		(1000000)	//ns to ms

static peripheral_pwm_h	g_pwm_h = NULL;

static int _get_duty_cycle(int mode, int *duty_cycle)
{
	if(mode == 0)
	{
		*duty_cycle = 2 * MILLI_SECOND;		//Counter-ClockWise
		LOGI("get_duty_cycle mode = [%d:%s]:duty_cycle[%d]ms",
				mode, "COUNTER-CLOCKWISE", *duty_cycle/(1000));
	}
	else if(mode == 1)
	{
		*duty_cycle = 1 * MILLI_SECOND;		//ClockWise
		LOGI("get_duty_cycle mode = [%d:%s]:duty_cycle[%d]ms",
				mode, "CLOCKWISE", *duty_cycle/(1000));
	}
	else if(mode == 2)
	{
		*duty_cycle = 1.5 * MILLI_SECOND;		//CENTER
		LOGI("get_duty_cycle mode = [%d:%s]:duty_cycle[%d]ms",
				mode, "CENTER", *duty_cycle/(1000));
	}
	else{
		LOGE("get_duty_cycle unknown mode=[%d]",mode);
		return -1;
	}

	return 0;
}

static peripheral_error_e resource_motor_driving(int mode)
{
	peripheral_error_e ret = PERIPHERAL_ERROR_NONE;

	int chip = ANCHOR3_PWM_CHIPID;	//Chip 0
	int pin = ANCHOR3_PWM_PIN;		//Pin 0

	int period = 20 * MILLI_SECOND;
	int duty_cycle = 0;
	bool enable = true;

	ret = _get_duty_cycle(mode, &duty_cycle);

	if(ret!=0){
		LOGE("_get_duty_cycle unknown mode=[%d]",mode);
		return PERIPHERAL_ERROR_INVALID_PARAMETER;
	}

	if(g_pwm_h == NULL ){
		// Opening a PWM Handle : The chip and pin parameters erquired for this funciton must be set
		if((ret = peripheral_pwm_open(chip, pin, &g_pwm_h))!=PERIPHERAL_ERROR_NONE){
			LOGE("peripheral_pwm_open() failed!![%d]",ret);
			return ret;
		}
	}

	//Setting the Period : sets the period to 20 milliseconds. The unit is nanoseconds
	if((ret=peripheral_pwm_set_period(g_pwm_h, period))!=PERIPHERAL_ERROR_NONE){
		LOGE("peripheral_pwm_set_period()failed!![%d]",ret);
		return ret;
	}

	//Setting the Duty Cycle : sets the duty cycle to 1~2 milliseconds. The unit is nanoseconds
	if((ret = peripheral_pwm_set_duty_cycle(g_pwm_h, duty_cycle))!=PERIPHERAL_ERROR_NONE){
		LOGE("peripheral_pwm_set_duty_cycle() failed!![%d]",ret);
		return ret;
	}

	//Enabling Repetition
	if((ret = peripheral_pwm_set_enabled(g_pwm_h, enable))!=PERIPHERAL_ERROR_NONE){
		LOGE("peripheral_pwm_set_enabled() failed!![%d]",ret);
		return ret;
	}

	if(g_pwm_h != NULL){
		//Closing a PWM Handle : close a PWM handle that is no longer used,
		if((ret = peripheral_pwm_close(g_pwm_h))!=PERIPHERAL_ERROR_NONE){
			LOGE("peripheral_pwm_close() failed!![%d]",ret);
			return ret;
		}
		g_pwm_h = NULL;
	}

	return ret;
}

void pwm_motor_test_main(void)
{
	int loop;

	/* PWM Servo Motor Test */
	for (loop=0; loop<5; loop++){
		resource_motor_driving(1);
		sleep(1);
		resource_motor_driving(0);
		sleep(1);
	}
}





