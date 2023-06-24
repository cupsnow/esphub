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
#include <esp_system.h>
#include <esp_wifi.h>
#include <esp_event.h>
#include <nvs_flash.h>

#include <lwip/err.h>
#include <lwip/sys.h>

#include <aloe_sys.h>

#include "dw_util.h"
#include "dw_looper.h"
#include "dw_sinsvc.h"
#include "eh_led.h"
#include "eh_btn.h"

#define log_d(...) dw_log_m("[Debug]", ##__VA_ARGS__)
#define log_e(...) dw_log_m("[ERROR]", ##__VA_ARGS__)
#define log_i(...) dw_log_m("[INFO]", ##__VA_ARGS__)

typedef enum {
	wifi_ophase_init = 0,
	wifi_ophase_sta_cfg,
	wifi_ophase_sta_start,
	wifi_ophase_sta_conn,
	wifi_ophase_sta_ip,
	wifi_ophase_sta_normal,
	wifi_ophase_sta_disconn,
	wifi_ophase_sta_ip2,
	wifi_ophase_sta_deinit,
} wifi_ophase_id_t;

static struct {
	// main looper
	dw_looper_t looper;
	aloe_thread_t tsk;

	// led
	float led1_duty100, led1_step;

	// btn
	dw_looper_msg_t btn_looper_msg;

    struct {
        aloe_sem_t lock;
        char buf[300];
    } logger;

    struct {
    	unsigned auth: 8;
    	int conn_trial, ophase;
    	esp_netif_t *sta_inst;
    	char ssid[32], pw[32];
    	esp_event_handler_instance_t wifi_evt_inst, ip_evt_inst;
    } wifi;

} eh_impl = {};

static int eh_wifi_sta_start(void);

void aloe_log_add_va(int lvl, const char *tag, long lno,
		const char *fmt, va_list va) {
    typeof(eh_impl.logger) *logger = &eh_impl.logger;
	size_t sz;

	if (aloe_sem_wait(&logger->lock, NULL, aloe_dur_infinite,
			"logger_add") != 0) {
		return;
	}
	sz = aloe_log_vfmsg(logger->buf, sizeof(logger->buf), lvl, tag, lno, fmt, va);
	if (sz > 0) printf(logger->buf);
	aloe_sem_post(&logger->lock, NULL, "logger_add");
}

static void aloe_logger_init(void) {
    typeof(eh_impl.logger) *logger = &eh_impl.logger;

    if (aloe_sem_init(&logger->lock, 1, 1, "logger_lock") != 0) {
    	log_e("Failed init logger lock\n");
    }
}

static int eh_looper_msg1_post_new(void (*handler)(dw_looper_msg_t*),
		const char *nm) {
	dw_looper_msg_t *looper_msg;

	if (!(looper_msg = aloe_mem_malloc(aloe_mem_id_stdc,
			sizeof(*looper_msg), nm))) {
		log_e("Failed create looper msg\n");
		return -1;
	}
	looper_msg->handler = handler;
	if (dw_looper_add(dw_looper_main, looper_msg, 0, NULL) != 0) {
		aloe_mem_free(looper_msg);
		log_e("Failed send looper msg\n");
		return -1;
	}
	return 0;
}

static void btn_triggered(dw_looper_msg_t *looper_msg) {
	int val = gpio_get_level(eh_btn1_gio);

	if (looper_msg != &eh_impl.btn_looper_msg) {
		log_e("Sanity check unexpect btn check\n");
		return;
	}

	eh_impl.btn_looper_msg.handler = NULL;

	if (val == 0) {
		eh_wifi_sta_start();
	}

	log_d("GPIO[%d] intr, val: %d\n", eh_btn1_gio, val);
}

static void btn_isr(void *args) {
	BaseType_t prio_woken = pdFALSE;

	if (eh_impl.btn_looper_msg.handler == NULL) {
		eh_impl.btn_looper_msg.handler = &btn_triggered;
		if (dw_looper_add(dw_looper_main, &eh_impl.btn_looper_msg,
                0, &prio_woken) != 0) {
			eh_impl.btn_looper_msg.handler = NULL;
		}
	}
	if (prio_woken) {
		portYIELD_FROM_ISR();
	}
}

static int eh_led_pulse(float inc) {
	if (inc == 0) inc = 1;
	if (aloe_abs(inc) != aloe_abs(eh_impl.led1_step)) {
		if (eh_impl.led1_step == 0) {
			eh_impl.led1_step = inc;
		} else {
			eh_impl.led1_step = (eh_impl.led1_step / eh_impl.led1_step)
					* aloe_abs(inc);
		}
	}
	eh_impl.led1_duty100 += eh_impl.led1_step;
	if (eh_impl.led1_duty100 <= 0) {
		eh_impl.led1_duty100 = 0;
		if (eh_impl.led1_step < 0) eh_impl.led1_step = 0 - eh_impl.led1_step;
	} else if (eh_impl.led1_duty100 >= 100) {
		eh_impl.led1_duty100 = 100;
		if (eh_impl.led1_step > 0) eh_impl.led1_step = 0 - eh_impl.led1_step;
	}
	eh_led1_set_bri(eh_impl.led1_duty100);
	return 0;
}

static void eh_wifi_feedback(dw_looper_msg_t *looper_msg) {

	if (eh_impl.wifi.ophase == wifi_ophase_sta_disconn) {
		log_d("wifi_ophase_sta_disconn\n");

		esp_wifi_stop();
		esp_wifi_deinit();
		if (eh_impl.wifi.sta_inst) {
			esp_netif_destroy_default_wifi(eh_impl.wifi.sta_inst);
			eh_impl.wifi.sta_inst = NULL;
		}
		eh_impl.wifi.ophase = wifi_ophase_init;
		goto finally;
	}

	if (eh_impl.wifi.ophase == wifi_ophase_sta_normal) {
		esp_netif_ip_info_t ipinfo;

		if (esp_netif_get_ip_info(esp_netif_get_default_netif(), &ipinfo) != ESP_OK) {
			log_e("Failed get ip info\n");
			goto finally;
		}

		log_d("wifi ready ip %d.%d.%d.%d, netmask %d.%d.%d.%d, gw: %d.%d.%d.%d\n",
				ESPIPADDR_PKARG(&ipinfo.ip), ESPIPADDR_PKARG(&ipinfo.netmask),
				ESPIPADDR_PKARG(&ipinfo.gw));

		dw_sinsvc_init();
		goto finally;
	}

	log_d("Sanity check unexpected wifi_ophase: %d\n", eh_impl.wifi.ophase);
finally:
	aloe_mem_free(looper_msg);
}

static void eh_wifi_event_handler(void *arg, esp_event_base_t event_base,
		int32_t event_id, void *event_data) {

    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
    	log_d("wifi started\n");

    	if (eh_impl.wifi.ophase != wifi_ophase_sta_start) {
    		log_e("Sanity check invalid ophase: %d\n", eh_impl.wifi.ophase);
    		return;
    	}
	    eh_impl.wifi.conn_trial = 3;
        eh_impl.wifi.ophase = wifi_ophase_sta_conn;
        esp_wifi_connect();
        return;
    }

    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
		wifi_event_sta_disconnected_t *evt =
				(wifi_event_sta_disconnected_t*)event_data;

		log_d("wifi ophase %d connecting failed reason code %d\n",
				eh_impl.wifi.ophase, evt->reason);

    	if (eh_impl.wifi.ophase == wifi_ophase_sta_conn) {
    		// connecting

            if (eh_impl.wifi.conn_trial > 0) {
            	log_d("wifi trial %d more connect to ap %s\n",
            			eh_impl.wifi.conn_trial,
            			(evt->ssid_len > 0 ? (char*)evt->ssid : ""));
            	eh_impl.wifi.conn_trial--;
                esp_wifi_connect();
                return;
            }

        	log_e("wifi failed connect to ap %s\n",
        			(evt->ssid_len > 0 ? (char*)evt->ssid : ""));
    	} else {
        	log_d("wifi disconnect to ap %s\n",
        			(evt->ssid_len > 0 ? (char*)evt->ssid : ""));
    	}
    	eh_impl.wifi.ophase = wifi_ophase_sta_disconn;

    	eh_looper_msg1_post_new(&eh_wifi_feedback, "wifi_conn");

    	return;
    }

    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_CONNECTED) {
    	wifi_event_sta_connected_t *evt =
    			(wifi_event_sta_connected_t*)event_data;

		eh_impl.wifi.ophase = wifi_ophase_sta_ip;
		log_d("wifi connected to ap %s, channel %d, requesting IP address\n",
				(evt->ssid_len > 0 ? (char*)evt->ssid : ""), (int)evt->channel);
		return;
    }

    if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
		ip_event_got_ip_t *evt = (ip_event_got_ip_t*)event_data;

		eh_impl.wifi.ophase = wifi_ophase_sta_normal;

		log_d("wifi got ip %d.%d.%d.%d, netmask %d.%d.%d.%d, gw: %d.%d.%d.%d\n",
				ESPIPADDR_PKARG(&evt->ip_info.ip),
				ESPIPADDR_PKARG(&evt->ip_info.netmask),
				ESPIPADDR_PKARG(&evt->ip_info.gw));

		eh_impl.wifi.conn_trial = 3;

    	eh_looper_msg1_post_new(&eh_wifi_feedback, "wifi_conn");

        return;
    }
}

static int eh_wifi_init(void) {
	ESP_ERROR_CHECK(esp_netif_init());
	ESP_ERROR_CHECK(esp_event_loop_create_default());
	ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
			ESP_EVENT_ANY_ID, &eh_wifi_event_handler, NULL,
			&eh_impl.wifi.wifi_evt_inst));
	ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
			IP_EVENT_STA_GOT_IP, &eh_wifi_event_handler, NULL,
			&eh_impl.wifi.ip_evt_inst));
	return 0;
}

static int eh_wifi_sta_start(void) {
	if (eh_impl.wifi.ophase == wifi_ophase_init) {
		if (!eh_impl.wifi.sta_inst) {
			eh_impl.wifi.sta_inst = esp_netif_create_default_wifi_sta();
		}
	    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
	    cfg.nvs_enable = 0;
	    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
	    eh_impl.wifi.ophase = wifi_ophase_sta_cfg;
	}
	if (eh_impl.wifi.ophase == wifi_ophase_sta_cfg) {
	    static wifi_config_t wifi_config = {
	        .sta = {
	            .ssid = "joe3",
	            .password = "joelaiamiami",
	            /* Authmode threshold resets to WPA2 as default if password matches WPA2 standards (pasword len => 8).
	             * If you want to connect the device to deprecated WEP/WPA networks, Please set the threshold value
	             * to WIFI_AUTH_WEP/WIFI_AUTH_WPA_PSK and set the password with length and format matching to
	             * WIFI_AUTH_WEP/WIFI_AUTH_WPA_PSK standards.
	             */
	            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
	            .sae_pwe_h2e = WPA3_SAE_PWE_HASH_TO_ELEMENT,
	            .sae_h2e_identifier = "",
	        },
	    };
	    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
	    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
	    eh_impl.wifi.conn_trial = 3;
	    eh_impl.wifi.ophase = wifi_ophase_sta_start;
	    ESP_ERROR_CHECK(esp_wifi_start());
	}
	return 0;
}

static void eh_looper_task(aloe_thread_t *args) {
#define looperDur 30
#define looperRound(_dur) (((_dur) + looperDur - 1) / looperDur)

#define outputHeapSizeDur 10000

	static int outputHeapSizeCountDown = 0;
	dw_looper_msg_t *msg;
	static size_t mz[2];

    log_d("eh_impl start\n");

	if (args != &eh_impl.tsk) {
		log_e("Sanity check invalid eh_impl\n");
		return;
	}
	while (!eh_impl.looper.quit) {

		if (outputHeapSizeCountDown > 0) {
			outputHeapSizeCountDown--;
		} else {
			mz[0] = xPortGetFreeHeapSize();
			log_d("xPortGetFreeHeapSize: %d\n", mz[0]);
			outputHeapSizeCountDown = looperRound(outputHeapSizeDur);
		}

		eh_led_pulse(0.5);

		msg = dw_looper_once(&eh_impl.looper, looperDur);
		if (msg) {
			if (msg->handler) (*msg->handler)(msg);
		}

	}
}

static void* eh_looper_start(void) {
	if (!dw_looper_init(&eh_impl.looper, 20)) {
		log_e("Failed create eh_looper\n");
		return NULL;
	}

	eh_impl.looper.ready = 1;

	if (aloe_thread_run(&eh_impl.tsk, &eh_looper_task, 10240,
			eh_task_prio1, "eh_looper") != 0) {
		log_e("Failed create looper task\n");
		return NULL;
	}
	return &eh_impl;
}

const char* eh_ver_str(void) {
	static const char *ver_str = "0.0.1";

	return ver_str;
}

void eh_reset(int dly) {
	for ( ; dly > 0; dly--) {
		log_d("Restarting in %d seconds...\n", dly);
		vTaskDelay(1000 / portTICK_PERIOD_MS);
	}
	log_d("Restarting now.\n");
	fflush(stdout);
	esp_restart();
}

void app_main(void) {
	esp_err_t esp_eno;

    memset(&eh_impl, 0, sizeof(eh_impl));

    //Initialize NVS
	esp_eno = nvs_flash_init();
	if (esp_eno == ESP_ERR_NVS_NO_FREE_PAGES
			|| esp_eno == ESP_ERR_NVS_NEW_VERSION_FOUND) {
		log_d("Erase nvs\n");
		ESP_ERROR_CHECK(nvs_flash_erase());
		esp_eno = nvs_flash_init();
	}
    ESP_ERROR_CHECK(esp_eno);

	aloe_logger_init();

	log_d("eh version: %s\n", eh_ver_str());
    
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

	eh_led1_init(0);
	eh_btn1_init(&btn_isr, NULL);
	eh_wifi_init();

	eh_looper_start();

}
