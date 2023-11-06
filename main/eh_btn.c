/* $Id$
 *
 * Copyright 2023, Joelai
 * All Rights Reserved.
 *
 * @author joelai
 */

#include "sdkconfig.h"

#include <aloe_sys.h>

#include "eh_btn.h"

#define log_m(_l, _f, _args...) do { \
	unsigned long tms = xTaskGetTickCount() * portTICK_PERIOD_MS; \
	printf("[%ld.%03ld]%s[%s][#%d] " _f, tms / 1000, tms % 1000, _l, __func__, __LINE__, ##_args); \
} while(0)

#define log_d(...) log_m("[Debug]", ##__VA_ARGS__)
#define log_e(...) log_m("[ERROR]", ##__VA_ARGS__)
#define log_i(...) log_m("[INFO]", ##__VA_ARGS__)

#define ESP_INTR_FLAG_DEFAULT 0

void eh_btn1_init(void (*isr)(void*), void *args) {
    gpio_config_t io_conf = {};

	io_conf.intr_type = GPIO_INTR_ANYEDGE;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = eh_btn1_pinsel;
    // io_conf.pull_up_en = 1;
    gpio_config(&io_conf);

	gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);
    gpio_isr_handler_add(eh_btn1_gio, isr, args);
}

button_handle_t eh_btn2_init(button_cb_t cb, void *usr_data) {
	// create gpio button
	button_config_t gpio_btn_cfg = {
	    .type = BUTTON_TYPE_GPIO,
	    .long_press_time = CONFIG_BUTTON_LONG_PRESS_TIME_MS,
	    .short_press_time = CONFIG_BUTTON_SHORT_PRESS_TIME_MS,
	    .gpio_button_config = {
	        .gpio_num = eh_btn1_gio,
	        .active_level = 0,
	    },
	};
	button_handle_t gpio_btn = iot_button_create(&gpio_btn_cfg);
	if(NULL == gpio_btn) {
	    log_e("Button create failed");
	    return NULL;
	}
	iot_button_register_cb(gpio_btn, BUTTON_SINGLE_CLICK, cb, usr_data);
	iot_button_register_cb(gpio_btn, BUTTON_DOUBLE_CLICK, cb, usr_data);
	iot_button_register_cb(gpio_btn, BUTTON_LONG_PRESS_UP, cb, usr_data);
	return gpio_btn;
}
