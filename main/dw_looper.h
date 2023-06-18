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

#ifndef _H_DK_DECKWIFI_NP_LOOPER
#define _H_DK_DECKWIFI_NP_LOOPER

#include <aloe_sys.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct dw_looper_rec {
	unsigned quit: 1;
	unsigned ready: 1;
	QueueHandle_t mq;
//	aloe_sem_t lock;
} dw_looper_t;

extern dw_looper_t *dw_looper_main;

void* dw_looper_init(dw_looper_t *looper, int cnt);

typedef struct dw_looper_msg_rec {
	void (*handler)(struct dw_looper_msg_rec*);
//	struct dw_looper_msg_rec *next, *prev;
} dw_looper_msg_t;

/** Add message to looper.
 *
 * @param rt A pointor to BaseType_t when caller from ISR @ref to xQueueSendFromISR
 */
int dw_looper_add(dw_looper_t*, dw_looper_msg_t*, long dur, void *rt);

dw_looper_msg_t* dw_looper_once(dw_looper_t *looper, long dur);

//void dw_looper_test1(void);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* _H_DK_DECKWIFI_NP_LOOPER */
