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

#ifndef _H_DK_DECKWIFI_NP_UTIL
#define _H_DK_DECKWIFI_NP_UTIL

#include <aloe_sys.h>

#ifdef __cplusplus
extern "C" {
#endif

#define dw_log_m(_l, _f, _args...) do { \
	unsigned long tms = xTaskGetTickCount() * portTICK_PERIOD_MS; \
	printf("[%ld.%03ld]%s[%s][#%d] " _f, tms / 1000, tms % 1000, _l, __func__, __LINE__, ##_args); \
} while(0)

#define eh_task_prio1 (tskIDLE_PRIORITY + 1)
#define eh_sinsvc_port 6000

#define ESPIPADDR_ENT(_ipinfo, _n) ((uint8_t*)&(_ipinfo)->addr)[_n]
#define ESPIPADDR_PKARG(_ipinfo) ESPIPADDR_ENT(_ipinfo, 0), \
	ESPIPADDR_ENT(_ipinfo, 1), ESPIPADDR_ENT(_ipinfo, 2), \
	ESPIPADDR_ENT(_ipinfo, 3)

#define DECKWIFI_THREAD_PRIO_DEF (eh_task_prio1)
#define DECKWIFI_THREAD_PRIO_SPIS (DECKWIFI_THREAD_PRIO_DEF)
#define DECKWIFI_THREAD_PRIO_SINSVC (DECKWIFI_THREAD_PRIO_DEF)
#define DECKWIFI_SOCKET_SVC_PORT eh_sinsvc_port
#define DW_SPI_TRUNK_SIZE 1024
#define SPI_BY_DMA 1

/** Get LP or HP. */
const char *dw_xp(int var);

void _dw_dump16(const void *data, size_t sz, const char *func, long lno,
		const char *fmt, ...);
#define dw_dump16(_data, _sz, _args...) _dw_dump16(_data, _sz, __func__, __LINE__, _args)


#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* _H_DK_DECKWIFI_NP_UTIL */
