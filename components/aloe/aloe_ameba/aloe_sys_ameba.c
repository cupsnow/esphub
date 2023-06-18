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

#include <aloe_sys.h>

ALOE_SYS_TEXT1_SECTION
static int snstrncpy(char *buf, size_t buf_sz, const char *name, size_t len) {
	if (len <= 0) len = name ? strlen(name) : 0;
	if (len > 0) {
		if (len >= buf_sz) len = buf_sz - 1;
		memcpy(buf, name, len);
	}
	buf[len] = '\0';
	return len;
}

#define snstrcpy(_p, _s, _n) snstrncpy(_p, _s, _n, -1)

ALOE_SYS_TEXT1_SECTION
int aloe_sem_init(aloe_sem_t *aloe_sem, int max, int cnt, const char *name) {
	if (max == 1) {
		if (!(aloe_sem->sem = xSemaphoreCreateBinary())) return -1;
		if (cnt >= max) xSemaphoreGive(aloe_sem->sem);
	} else {
		if (!(aloe_sem->sem = xSemaphoreCreateCounting((UBaseType_t)max,
				(UBaseType_t)cnt))) {
			return -1;
		}
	}
#if defined(aloe_sem_name_size) && aloe_sem_name_size > 0
	if (name != aloe_sem->name) {
		snstrcpy(aloe_sem->name, aloe_sem_name_size, name);
	}
	aloe_sem->max = max;
	aloe_sem->cnt = cnt;
#endif
	return 0;
}

ALOE_SYS_TEXT1_SECTION
void aloe_sem_post(aloe_sem_t *aloe_sem, void *rt,
		const char *name __attribute__((unused))) {
	if (rt) {
		xSemaphoreGiveFromISR(aloe_sem->sem, (BaseType_t*)rt);
	} else {
		xSemaphoreGive(aloe_sem->sem);
	}
}

ALOE_SYS_TEXT1_SECTION
int aloe_sem_wait(aloe_sem_t *aloe_sem, void *rt, long dur,
		const char *name __attribute__((unused))) {
	BaseType_t eno;

	if (rt) {
		eno = xSemaphoreTakeFromISR(aloe_sem->sem, (BaseType_t*)rt);
	} else {
		eno = xSemaphoreTake(aloe_sem->sem, aloe_msDur(dur));
	}
	return eno == pdTRUE ? 0 : -1;
}

ALOE_SYS_TEXT1_SECTION
void aloe_sem_destroy(aloe_sem_t *aloe_sem) {
	vSemaphoreDelete(aloe_sem->sem);
}

ALOE_SYS_TEXT1_SECTION
int aloe_thread_run(aloe_thread_t *aloe_thread, void(*run)(aloe_thread_t*),
		size_t stack, int prio, const char *name) {
#if defined(aloe_thread_name_size) && aloe_thread_name_size > 0
	if (name != aloe_thread->name) {
		snstrcpy(aloe_thread->name, aloe_thread_name_size, name);
	}
#endif
	if (xTaskCreate((TaskFunction_t)run, name, stack, aloe_thread,
			(UBaseType_t)prio, &aloe_thread->thread) != pdPASS) {
		return -1;
	}
	return 0;
}
