/* $Id$
 *
 * Copyright 2023, Joelai
 * All Rights Reserved.
 *
 * @author joelai
 */

#include "sdkconfig.h"

#include <aloe_sys.h>

#include "eh_led.h"

#define log_m(_l, _f, _args...) do { \
	unsigned long tms = xTaskGetTickCount() * portTICK_PERIOD_MS; \
	printf("[%ld.%03ld]%s[%s][#%d] " _f, tms / 1000, tms % 1000, _l, __func__, __LINE__, ##_args); \
} while(0)

#define log_d(...) log_m("[Debug]", ##__VA_ARGS__)
#define log_e(...) log_m("[ERROR]", ##__VA_ARGS__)
#define log_i(...) log_m("[INFO]", ##__VA_ARGS__)

void eh_led1_set_bri(float _duty100) {
	ESP_ERROR_CHECK(ledc_set_duty(eh_led1_mode, eh_led1_ch, eh_led1_duty(_duty100)));
	ESP_ERROR_CHECK(ledc_update_duty(eh_led1_mode, eh_led1_ch));
}

void eh_led1_init(float duty100) {
	ledc_timer_config_t ledc_timer = {.speed_mode = eh_led1_mode,
			.timer_num = LEDC_TIMER_0, .duty_resolution = eh_led1_duty_res,
			.freq_hz = 5000, .clk_cfg = LEDC_AUTO_CLK};

	ledc_channel_config_t ledc_channel = {.speed_mode = eh_led1_mode,
			.channel = eh_led1_ch, .timer_sel = ledc_timer.timer_num,
			.intr_type = LEDC_INTR_DISABLE, .gpio_num = 5,
			.duty = eh_led1_duty(duty100), .hpoint = 0};

	ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));
	ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel));
}

