/* $Id$
 *
 * Copyright 2023, Joelai
 * All Rights Reserved.
 *
 * @author joelai
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <inttypes.h>
#include <math.h>

#include <sdkconfig.h>

#include "dw_spi.h"

#define log_e(...) aloe_log_e(__VA_ARGS__)
#define log_d(...) aloe_log_d(__VA_ARGS__)
#define log_i(...) aloe_log_add(aloe_log_level_info, __func__, __LINE__, __VA_ARGS__)

typedef struct {
	int type;
} mq_msg_t;

#define mq_msg_id_spi_send ((mq_msg_t*)1)
#define mq_msg_id_spi_recv ((mq_msg_t*)2)
#define mq_msg_id_spi_req_done ((mq_msg_t*)3)

static struct {
	unsigned ready: 2;
	unsigned quit: 1;

	aloe_thread_t tsk;
	aloe_sem_t lock;
	QueueHandle_t mq;

	volatile char spis_tx_done, spis_rx_done, spim_tx_done, spim_rx_done;

	struct {
		dw_spi2_req_t *req, *req_recycle;
		aloe_buf_t fb;
	} req_proc;

	dw_spi2_req_list_t req_list;

	// 32 bytes align
	char *xfer, *xfer_alloc;

	struct {
		unsigned recycle_corrupt;
	} st;

} impl = {};

#define spi2_req_gc(_req) do { \
	if ((_req)->cb) { \
		(*(_req)->cb)((_req)->cbarg); \
	} \
	(_req) = NULL; \
} while(0)

dw_spi2_req_t* dw_spi2_req_pop_isr(dw_spi2_req_list_t *req_list,
		aloe_sem_t *lock, BaseType_t *rt) {
	dw_spi2_req_t *req = NULL;

	if (aloe_sem_wait(lock, rt, aloe_dur_infinite, "spi2") != 0) {
		log_e("lock\n");
		return NULL;
	}

	if ((req = TAILQ_FIRST(req_list))) {
		TAILQ_REMOVE(req_list, req, qent);
	}
	aloe_sem_post(lock, rt, "spi2");
	return req;
}

int dw_spi2_req_add_isr(dw_spi2_req_list_t *req_list, aloe_sem_t *lock,
		dw_spi2_req_t *req, BaseType_t *rt) {
	if (aloe_sem_wait(lock, rt, aloe_dur_infinite, "spi2") != 0) {
		log_e("lock\n");
		return -1;
	}
	TAILQ_INSERT_TAIL(req_list, req, qent);
	aloe_sem_post(lock, rt, "spi2");
	return 0;
}

int dw_spi2_req_is_empty_isr(dw_spi2_req_list_t *req_list, aloe_sem_t *lock,
		BaseType_t *rt) {
	int e;

	if (aloe_sem_wait(lock, rt, aloe_dur_infinite, "spi2") != 0) {
		log_e("lock\n");
		return -1;
	}
	e = TAILQ_EMPTY(req_list);
	aloe_sem_post(lock, rt, "spi2");
	return e;
}

int dw_spi2_add(dw_spi2_req_t *req) {
	mq_msg_t *msg = mq_msg_id_spi_send;

	if (dw_spi2_req_add(&impl.req_list, &impl.lock, req) != 0) {
		return -1;
	}

	if (xQueueSend(impl.mq, &msg, 100) != pdPASS) {
		log_e("The queue to notify SPI is full\n");
	}
	return 0;
}

static int dw_spi2_send_start(const void *data, size_t sz) {
	if (impl.req_proc.req && impl.req_proc.fb.data == data) {
		mq_msg_t *msg;

		impl.req_proc.req_recycle = impl.req_proc.req;
		impl.req_proc.req = NULL;
		msg = mq_msg_id_spi_req_done;
		if (xQueueSend(impl.mq, &msg, portMAX_DELAY) != pdPASS) {
//			log_e("The queue to notify SPI is full\n");
		}
	}
	return sz;
}

static int dw_spi2_send_done(void) {
	return 0;
}

int dw_spi2_send(const void *data, size_t sz) {
	int r, rs;

	if ((rs = dw_spi2_send_start(data, sz)) <= 0) return rs;
	if ((r = dw_spi2_send_done()) != 0) return -1;
	return rs;
}

int dw_spi2_send2(const void *data, size_t sz) {
	int r, rs;

	if ((r = dw_spi2_send_done()) != 0) return r;
	if ((rs = dw_spi2_send_start(data, sz)) <= 0) return rs;
	return rs;
}

void spi2_req_proc2(dw_spi2_req_t *req) {
	int r;

	do {
		if (req->sz <= 0) break;

		while (impl.req_proc.req) {
			aloe_thread_sleep(1);
		}

		if (impl.req_proc.req) {
			log_e("Previous SPI not finish\n");
			break;
		}
		if (impl.req_proc.req_recycle) {
//			log_d("SPI req gc\n");
			spi2_req_gc(impl.req_proc.req_recycle);
		}

		impl.req_proc.req = req;
		impl.req_proc.fb.data = (void*)req->data;
		impl.req_proc.fb.lmt = impl.req_proc.fb.cap = req->sz;
		impl.req_proc.fb.pos = aloe_min(DW_SPI_TRUNK_SIZE, req->sz);
		if ((r = dw_spi2_send_start(req->data, impl.req_proc.fb.pos)) <= 0) {
			log_e("Failed start send, err: %d\n", r);
			impl.req_proc.req = NULL;
			break;
		}
		// hand over to isr
		req = NULL;
	} while(0);

	if (req && req->cb) (*req->cb)(req->cbarg);
}

static void spi2_slave_task(aloe_thread_t *args) {
	mq_msg_t *msg;
	dw_spi2_req_t *req;

	(void)args;

	log_d("SPI slave start, trunk size: %d, SPI2%s\n",
			DW_SPI_TRUNK_SIZE,
			", mock api"
			);

	while (!impl.quit) {
		if (xQueueReceive(impl.mq, &msg, aloe_msDur(1000)) != pdPASS) {
			msg = NULL;
			if (impl.st.recycle_corrupt) {
				log_e("recycle_corrupt: %d\n", impl.st.recycle_corrupt);
//				impl.st.recycle_corrupt = 0;
			}
		}
		if (impl.req_proc.req_recycle) {
//			log_d("SPI req gc\n");
			spi2_req_gc(impl.req_proc.req_recycle);
		}
		if (msg) {
			while ((req = dw_spi2_req_pop(&impl.req_list, &impl.lock))) {
				spi2_req_proc2(req);
			}
		}
	}
	vTaskDelete(NULL);
}

int dw_spi2_start(unsigned master, unsigned clkDiv) {

	(void)master;
	(void)clkDiv;

	if (impl.ready) {
		log_e("Already initialized\n");
		return -1;
	}

	memset(&impl, 0, sizeof(impl));
	TAILQ_INIT(&impl.req_list);
	impl.spis_tx_done = impl.spis_rx_done = 1;
	impl.spim_tx_done = impl.spim_rx_done = 1;

	if (!(impl.xfer_alloc = (void*)aloe_mem_malloc(aloe_mem_id_psram,
			32 + DW_SPI_TRUNK_SIZE,
			"spi2"))) {
		log_e("alloc buffer\n");
		return -1;
	}
	impl.xfer = (void*)aloe_roundup((unsigned long)impl.xfer_alloc, 32);

	if (!(impl.mq = xQueueCreate(20, sizeof(mq_msg_t*)))) {
		log_e("Failed alloc mq\n");
		aloe_mem_free(impl.xfer_alloc);
		return -1;
	}

	if (aloe_sem_init(&impl.lock, 1, 1, "spi2") != 0) {
		log_e("Failed init lock\n");
		vQueueDelete(impl.mq);
		aloe_mem_free(impl.xfer_alloc);
		return -1;
	}

	if (aloe_thread_run(&impl.tsk,
			&spi2_slave_task,
			2048, DECKWIFI_THREAD_PRIO_SPIS, "spi2_slv") != 0) {
		log_e("Failed start looper\n");
		vQueueDelete(impl.mq);
		aloe_mem_free(impl.xfer_alloc);
		aloe_sem_destroy(&impl.lock);
		return -1;
	}
	impl.ready = 1;
	return 0;
}


