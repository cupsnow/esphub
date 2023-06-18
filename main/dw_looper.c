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

#include "dw_looper.h"
#include "dw_util.h"

#define log_d(_fmt...) dw_log_m("[Debug]", _fmt)
#define log_e(_fmt...) dw_log_m("[ERROR]", _fmt)

dw_looper_t *dw_looper_main = NULL;

ALOE_SYS_TEXT1_SECTION
void* dw_looper_init(dw_looper_t *looper, int cnt) {
//	if (aloe_sem_init(&looper->lock, 1, 1, "loop_lock") != 0) {
//		log_e("Failed alloc looper lock\n");
//		return NULL;
//	}
	if (!(looper->mq = xQueueCreate(cnt, sizeof(dw_looper_msg_t*)))) {
		log_e("Failed alloc looper mq\n");
//		aloe_sem_destroy(&looper->lock);
		return NULL;
	}
	if (!dw_looper_main) {
		log_d("Set main looper\n");
		dw_looper_main = looper;
	}
	return looper;
}

ALOE_SYS_TEXT1_SECTION
int dw_looper_add(dw_looper_t *looper, dw_looper_msg_t *msg, long dur,
		void *rt) {
	BaseType_t eno;

	if (!looper || !looper->ready) return -1;
	if (rt) {
		eno = xQueueSendFromISR(looper->mq, &msg, (BaseType_t*)rt);
	} else {
		eno = xQueueSend(looper->mq, &msg, aloe_msDur(dur));
	}
	return eno == pdTRUE ? 0 : -1;
}

ALOE_SYS_TEXT1_SECTION
dw_looper_msg_t* dw_looper_once(dw_looper_t *looper, long dur) {
	dw_looper_msg_t *msg = NULL;

	if (looper && looper->ready && xQueueReceive(looper->mq, &msg,
			aloe_msDur(dur)) == pdPASS) {
		return msg;
	}
	return NULL;
}

#if 0
typedef struct {
	dw_looper_msg_t looper_msg;
	int cnt;
} looper_test_t;

static looper_test_t looper_test = { };

ALOE_SYS_TEXT1_SECTION
static void looper_test_handler(dw_looper_msg_t *looper_msg) {
	looper_test_t *looper_test = aloe_container_of(looper_msg, looper_test_t,
			looper_msg);
	log_d("enter: %d\n", looper_test->cnt);
	looper_test->cnt++;
	looper_msg->handler = NULL;
}

ALOE_SYS_TEXT1_SECTION
void dw_looper_test1(void) {
	if (!looper_test.looper_msg.handler) {
		looper_test.looper_msg.handler = &looper_test_handler;
		log_d("send looper test: %d\n", looper_test.cnt);
		dw_looper_add(dw_looper_main, &looper_test.looper_msg,
				aloe_dur_infinite, NULL);
	}
}
#endif
