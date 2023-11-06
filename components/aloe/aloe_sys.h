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

/** @defgroup ALOE_SYS System
 * @ingroup ALOE
 * @brief HAL
 */

#ifndef _H_ALOE_SYS
#define _H_ALOE_SYS

#if defined(ALOE_SYS_AMEBA_DX)
#  include "aloe_sys_ameba_dx.h"
#elif defined(ALOE_SYS_AMEBA_LP)
#  include "aloe_sys_ameba_lp.h"
#elif defined(ALOE_SYS_LINUX)
#  include "aloe_sys_linux.h"
#elif defined(ALOE_SYS_ESP32)
#  include "aloe_sys_esp32.h"
#endif

#ifndef ALOE_SYS_TEXT1_SECTION
#  define ALOE_SYS_TEXT1_SECTION
#endif

#ifndef ALOE_SYS_BSS1_SECTION
#  define ALOE_SYS_BSS1_SECTION
#endif

#ifndef ALOE_SYS_DATA1_SECTION
#  define ALOE_SYS_DATA1_SECTION
#endif

#include "aloe_util.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @addtogroup ALOE_SYS
 * @{
 */

#ifndef aloe_endl
#  define aloe_endl aloe_endl_msw
#endif

/** Infinite duration. */
#define aloe_dur_infinite (-1lu)

/** Response immediately. */
#define aloe_dur_zero (0lu)

typedef struct aloe_timeval_rec {
	unsigned long tv_sec, tv_usec;
} aloe_timeval_t;

/** @} ALOE_SYS */

// #define aloe_sem_name_size 10

typedef struct aloe_sem_rec aloe_sem_t;
int aloe_sem_init(aloe_sem_t*, int max, int cnt, const char *name);
void aloe_sem_post(aloe_sem_t*, void *rt, const char *name);
int aloe_sem_wait(aloe_sem_t*, void *rt, long dur, const char *name);
void aloe_sem_destroy(aloe_sem_t*);

// #define aloe_thread_name_size 10
typedef struct aloe_thread_rec aloe_thread_t;
int aloe_thread_run(aloe_thread_t*, void(*)(aloe_thread_t*), size_t stack,
		int prio, const char *name);

// void aloe_thread_sleep(_ms);

// unsigned long aloe_ticks(void);
// unsigned long aloe_tick2ms(_ts);
// unsigned long aloe_ms2tick(_ms);

typedef enum aloe_mem_id_enum {
	aloe_mem_id_stdc,
	aloe_mem_id_dxmem,
	aloe_mem_id_psram,
	aloe_mem_id_sig
} aloe_mem_id_t;

// __attribute__((packed)) cause crash on ameba
typedef struct /* __attribute__((packed)) */ aloe_mem_rec {
	const aloe_mem_id_t *sig;
	aloe_mem_id_t id;
	size_t sz;
} aloe_mem_t;

extern const aloe_mem_id_t aloe_mem_sig;

void* aloe_mem_malloc(aloe_mem_id_t, size_t, const char *name);
void* aloe_mem_calloc(aloe_mem_id_t, size_t, size_t, const char *name);
int aloe_mem_free(void*);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* _H_ALOE_SYS */
