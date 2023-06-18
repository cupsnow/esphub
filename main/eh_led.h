/* $Id$
 *
 * Copyright 2023, Joelai
 * All Rights Reserved.
 *
 * @author joelai
 */

#ifndef MAIN_EH_LED_H_
#define MAIN_EH_LED_H_

#include <driver/ledc.h>
#include <math.h>

#ifdef __cplusplus
extern "C" {
#endif

#define eh_led1_duty_res (LEDC_TIMER_13_BIT)
#define eh_led1_duty_max ((1 << eh_led1_duty_res) - 1)
#define eh_led1_mode (LEDC_LOW_SPEED_MODE)
#define eh_led1_ch (LEDC_CHANNEL_0)

#define eh_led1_duty(_duty100) ((_duty100) <= 0.0 ? 0 : \
		(_duty100) >= 100.0 ? eh_led1_duty_max : \
		(unsigned)round((double)(_duty100) * eh_led1_duty_max / 100))

void eh_led1_set_bri(float _duty100);
void eh_led1_init(float duty100);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* MAIN_EH_LED_H_ */
