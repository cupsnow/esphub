/* $Id$
 *
 * Copyright 2023, Joelai
 * All Rights Reserved.
 *
 * @author joelai
 */

#ifndef _H_EM_BTN
#define _H_EM_BTN

#include <driver/gpio.h>
#include <iot_button.h>

#ifdef __cplusplus
extern "C" {
#endif

#define eh_btn1_gio (GPIO_NUM_0)
#define eh_btn1_pinsel (1ull << eh_btn1_gio)

void eh_btn1_init(void (*isr)(void*), void *args);
button_handle_t eh_btn2_init(button_cb_t cb, void *usr_data);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* _H_EM_BTN */
