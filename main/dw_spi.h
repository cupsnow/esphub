/* $Id$
 *
 * Copyright 2023, Joelai
 * All Rights Reserved.
 *
 * @author joelai
 */

#ifndef MAIN_DW_SPI_H_
#define MAIN_DW_SPI_H_

#include <aloe_sys.h>

#include "dw_util.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct dw_spi2_req_rec {
	const void *data;
	size_t sz;
	void (*cb)(void*);
	void *cbarg;
	TAILQ_ENTRY(dw_spi2_req_rec) qent;
} dw_spi2_req_t;

typedef TAILQ_HEAD(, dw_spi2_req_rec) dw_spi2_req_list_t;

dw_spi2_req_t* dw_spi2_req_pop_isr(dw_spi2_req_list_t *req_list,
		aloe_sem_t *lock, BaseType_t *rt);
int dw_spi2_req_add_isr(dw_spi2_req_list_t *req_list, aloe_sem_t *lock,
		dw_spi2_req_t *req, BaseType_t *rt);
int dw_spi2_req_is_empty_isr(dw_spi2_req_list_t *req_list, aloe_sem_t *lock,
		BaseType_t *rt);

#define dw_spi2_req_pop(_list, _lock) \
	dw_spi2_req_pop_isr(_list, _lock, NULL)
#define dw_spi2_req_add(_list, _lock, _req) \
	dw_spi2_req_add_isr(_list, _lock, _req, NULL)
#define dw_spi2_req_is_empty(_list, _lock) \
	dw_spi2_req_is_empty_isr(_list, _lock, NULL)

int dw_spi2_start(unsigned master, unsigned clkDiv);
int dw_spi2_add(dw_spi2_req_t*);

/**
 * thread unsafe
 *
 * @return
 *   > 0 when successful
 *   <= 0 when failed
 */
int dw_spi2_send(const void*, size_t);

#define _dw_spi2_send(_d, _sz) \
		(dw_spi2_send_start(_d, _sz) > 0 && dw_spi2_send_done() == 0)

/**
 * thread unsafe
 * check previously done before send data
 *
 * @return
 *   > 0 when successful
 *   <= 0 when failed
 */
int dw_spi2_send2(const void*, size_t);

#define _dw_spi2_send2(_d, _sz) \
		(dw_spi2_send_done() == 0 && dw_spi2_send_start(_d, _sz) > 0)


#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* MAIN_DW_SPI_H_ */
