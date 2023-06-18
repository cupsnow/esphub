/* $Id$
 *
 * Copyright 2023, Dexatek Technology Ltd.
 * This is proprietary information of Dexatek Technology Ltd.
 * All Rights Reserved. Reproduction of this documentation or the
 * accompanying programs in any manner whatsoever without the written
 * permission of Dexatek Technology Ltd. is strictly forbidden.
 *
 * @author joelai
 */

#ifndef _H_ALOE_SYS_AMEBA
#define _H_ALOE_SYS_AMEBA

#ifndef _H_ALOE_SYS
#  error "Please included <aloe/sys.h> instead!"
#endif

#include <platform_stdlib.h>
#include <FreeRTOS.h>
#include <semphr.h>
#include <task.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef aloe_endl
#  define aloe_endl aloe_endl_unix
#endif

#define aloe_sem_name_size 10
struct aloe_sem_rec {
	SemaphoreHandle_t sem;
#if defined(aloe_sem_name_size) && aloe_sem_name_size > 0
	char name[aloe_sem_name_size];
	int max, cnt;
#endif
};

#define aloe_thread_name_size 10
struct aloe_thread_rec  {
	TaskHandle_t thread;
#if defined(aloe_thread_name_size) && aloe_thread_name_size > 0
	char name[aloe_thread_name_size];
#endif
};

// void aloe_thread_sleep(_ms);
#define aloe_thread_sleep(_ms) vTaskDelay(aloe_msDur(_ms))

//unsigned long aloe_ticks(void);
#define aloe_ticks() ((unsigned long)xTaskGetTickCount())

//unsigned long aloe_tick2ms(_ts);
#define aloe_tick2ms(_ts) ((_ts) * portTICK_PERIOD_MS)

//unsigned long aloe_ms2tick(_ms);
#define aloe_ms2tick(_ms) ((_ms) / portTICK_PERIOD_MS)

#define aloe_msDur(_ms) ((_ms) == 0 ? 0 : \
		(unsigned long)(_ms) == aloe_dur_infinite ? portMAX_DELAY : \
		(_ms) < 0 ? portMAX_DELAY : \
		(TickType_t)(_ms) < portTICK_PERIOD_MS ? 1 : \
		(_ms) / portTICK_PERIOD_MS)

int aloe_sem_lock(struct aloe_sem_rec *aloe_sem, void *rt, int sw, long dur);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* _H_ALOE_SYS_AMEBA */
