/*
 * SPDX-FileCopyrightText: 2010-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <inttypes.h>
#include <math.h>

#include <sdkconfig.h>

#include <esp_chip_info.h>
#include <esp_flash.h>

#include <aloe_sys.h>

#include "dw_util.h"
#include "dw_looper.h"
#include "em1_led.h"

#define log_m(_l, _f, _args...) do { \
	unsigned long tms = xTaskGetTickCount() * portTICK_PERIOD_MS; \
	printf("[%ld.%03ld]%s[%s][#%d] " _f, tms / 1000, tms % 1000, _l, __func__, __LINE__, ##_args); \
} while(0)

#define log_d(...) log_m("[Debug]", ##__VA_ARGS__)
#define log_e(...) log_m("[ERROR]", ##__VA_ARGS__)
#define log_i(...) log_m("[INFO]", ##__VA_ARGS__)

#define em1_task_prio1 (tskIDLE_PRIORITY + 1)

/* LOGGER
 *
 */
static struct {
	aloe_sem_t lock;
	char buf[300];
} logger;

void aloe_log_add_va(int lvl, const char *tag, long lno,
		const char *fmt, va_list va) {
	size_t sz;

	if (aloe_sem_wait(&logger.lock, NULL, aloe_dur_infinite,
			"logger_add") != 0) {
		return;
	}
	sz = aloe_log_vfmsg(logger.buf, sizeof(logger.buf), lvl, tag, lno, fmt, va);
	if (sz > 0) printf(logger.buf);
	aloe_sem_post(&logger.lock, NULL, "logger_add");
}

static void aloe_logger_init(void) {
	aloe_sem_init(&logger.lock, 1, 1, "logger_lock");
}

/* MAIN_LOOPER
 *
 */
static struct {
	dw_looper_t looper;
	aloe_thread_t tsk;

	float led1_duty100;

} main_looper = {};

static void main_looper_run(aloe_thread_t *args) {
#define looperDur 30
#define outputHeapSizeDur 10000
	static int outputHeapSizeCountDown = (outputHeapSizeDur + looperDur - 1) / looperDur;
	dw_looper_msg_t *msg;
	int bri_inc = 3;

	if (args != &main_looper.tsk) {
		log_e("Sanity check invalid main_looper\n");
		return;
	}
	while (!main_looper.looper.quit) {
		if (outputHeapSizeCountDown > 0) {
			outputHeapSizeCountDown--;
		} else {
			aloe_log_d("xPortGetFreeHeapSize: %d\n", xPortGetFreeHeapSize());
			outputHeapSizeCountDown = (outputHeapSizeDur + looperDur - 1) / looperDur;
		}

		msg = dw_looper_once(&main_looper.looper, looperDur);

		if (msg) {
			if (msg->handler) (*msg->handler)(msg);
		}

		main_looper.led1_duty100 += bri_inc;
		if (main_looper.led1_duty100 <= 0) {
			main_looper.led1_duty100 = 0;
			bri_inc = 3;
		} else if (main_looper.led1_duty100 >= 100) {
			main_looper.led1_duty100 = 100;
			bri_inc = -3;
		}
		em1_led1_set_bri(main_looper.led1_duty100);
	}
}

static void* main_looper_start(void) {
	if (main_looper.looper.ready) {
		log_e("main_looper already initialized\n");
		return NULL;
	}

	memset(&main_looper, 0, sizeof(main_looper));

	if (!dw_looper_init(&main_looper.looper, 20)) {
		log_e("Failed create main_looper\n");
		return NULL;
	}

	if (aloe_thread_run(&main_looper.tsk, &main_looper_run, 2048,
			em1_task_prio1, "aloe_tsk1") != 0) {
		log_e("Failed create looper task\n");
		return NULL;
	}
	main_looper.looper.ready = 1;
	return &main_looper;
}

const char* em1_ver_str(void) {
	static const char *ver_str = "0.0.1";

	return ver_str;
}

void em1_reset(int dly) {
	for ( ; dly > 0; dly--) {
		log_d("Restarting in %d seconds...\n", dly);
		vTaskDelay(1000 / portTICK_PERIOD_MS);
	}
	log_d("Restarting now.\n");
	fflush(stdout);
	esp_restart();
}

void app_main(void) {
	aloe_logger_init();

	log_d("em1 start ver: %s\n", em1_ver_str());

	/* Print chip information */
	esp_chip_info_t chip_info;
	uint32_t flash_size;
	esp_chip_info(&chip_info);
	log_i("This is %s chip with %d CPU core(s), WiFi%s%s%s\n", CONFIG_IDF_TARGET,
			chip_info.cores, (chip_info.features & CHIP_FEATURE_BT) ? "/BT" : "",
			(chip_info.features & CHIP_FEATURE_BLE) ? "/BLE" : "",
			(chip_info.features & CHIP_FEATURE_IEEE802154) ?
					", 802.15.4 (Zigbee/Thread)" : "");

	unsigned major_rev = chip_info.revision / 100;
	unsigned minor_rev = chip_info.revision % 100;
	log_i("silicon revision v%d.%d\n", major_rev, minor_rev);
	if (esp_flash_get_size(NULL, &flash_size) != ESP_OK) {
		log_e("Get flash size failed\n");
		return;
	}

	log_i("%" PRIu32 "MB %s flash\n", flash_size / (uint32_t)(1024 * 1024),
			(chip_info.features & CHIP_FEATURE_EMB_FLASH) ? "embedded" : "external");

	log_i("Minimum free heap size: %" PRIu32 " bytes\n", esp_get_minimum_free_heap_size());

	em1_led1_init(0);

	main_looper_start();

}
