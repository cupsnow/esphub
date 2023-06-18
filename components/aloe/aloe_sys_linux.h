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

#ifndef _H_ALOE_SYS_LINUX
#define _H_ALOE_SYS_LINUX

#ifndef _H_ALOE_SYS
#  error "Please included <aloe/sys.h> instead!"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>
#include <unistd.h>
#include <sys/time.h>
#include <pthread.h>
#include <sched.h>
#include <time.h>
#include <semaphore.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef aloe_endl
#  define aloe_endl aloe_endl_unix
#endif

unsigned long aloe_ticks(void);
#define aloe_tick2ms(_ts) ((_ts) / aloe_10e3)
#define aloe_ms2tick(_ms) ((_ms) * aloe_10e3)

#define aloe_sem_name_size 20
struct aloe_sem_rec {
	pthread_mutex_t mutex;
#if aloe_sem_name_size
	char name[aloe_sem_name_size];
#endif
	int max, cnt;
	pthread_cond_t not_empty;
};

#define aloe_thread_name_size 20
struct aloe_thread_rec {
	pthread_t thread;
	void (*run)(struct aloe_thread_rec*);
#if aloe_thread_name_size
	char name[aloe_thread_name_size];
#endif
};

#define aloe_thread_sleep(_ms) usleep(_ms * 1000)

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* _H_ALOE_SYS_LINUX */
