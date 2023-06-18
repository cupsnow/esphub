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

#if 1

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

static void timespec_dur_ms(struct timespec *tv, unsigned long dur_ms) {
	tv->tv_sec += (dur_ms / aloe_10e3);
	tv->tv_nsec += ((dur_ms % aloe_10e3) * aloe_10e6);
	while (tv->tv_nsec >= aloe_10e9) {
		tv->tv_sec++;
		tv->tv_nsec -= aloe_10e9;
	}
}

#define aloe_msDur(_ms) ((_ms) == 0 ? aloe_dur_zero : \
		(_ms) < 0 ? aloe_dur_infinite : \
		(_ms))

static int mutex_lock(pthread_mutex_t *mutex, unsigned long dur_ms) {
	struct timespec tv;

	if (dur_ms == aloe_dur_zero) return pthread_mutex_trylock(mutex);
	if (dur_ms == aloe_dur_infinite) return pthread_mutex_lock(mutex);

	if (clock_gettime(CLOCK_REALTIME, &tv) != 0) return errno;
	timespec_dur_ms(&tv, dur_ms);
	return pthread_mutex_timedlock(mutex, &tv);
}

static int cond_wait(pthread_cond_t *cond, pthread_mutex_t *mutex,
		unsigned long dur_ms) {
	struct timespec tv;

	if (dur_ms == aloe_dur_infinite) return pthread_cond_wait(cond, mutex);

	if (clock_gettime(CLOCK_REALTIME, &tv) != 0) return errno;
	if (dur_ms != aloe_dur_zero) timespec_dur_ms(&tv, dur_ms);
	return pthread_cond_timedwait(cond, mutex, &tv);
}

unsigned long aloe_ticks(void) {
	struct timespec tv;

	clock_gettime(CLOCK_REALTIME, &tv);
	return tv.tv_sec * aloe_ms2tick(aloe_10e3) +
			aloe_ms2tick(tv.tv_nsec / aloe_10e3) / aloe_10e3;
}

int aloe_sem_init(aloe_sem_t *ctx, int max, int cnt, const char *name) {
	int r;

	if ((r = pthread_mutex_init(&ctx->mutex, NULL)) != 0) {
		return r;
	}
	if ((r = pthread_cond_init(&ctx->not_empty, NULL)) != 0) {
		pthread_mutex_destroy(&ctx->mutex);
		return r;
	}
	ctx->max = max;
	ctx->cnt = cnt;
#if defined(aloe_sem_name_size) && aloe_sem_name_size > 0
	if (name != ctx->name) {
		snstrcpy(ctx->name, aloe_sem_name_size, name);
	}
#endif
	return 0;
}

void aloe_sem_post(aloe_sem_t *ctx, void *rt, const char *name) {
	char broadcast = 0;

	(void)rt;

	mutex_lock(&ctx->mutex, aloe_dur_infinite);
	if ((ctx->cnt < ctx->max) && ((++(ctx->cnt)) == 1)) {
		if (broadcast) pthread_cond_broadcast(&ctx->not_empty);
		else pthread_cond_signal(&ctx->not_empty);
	}
	pthread_mutex_unlock(&ctx->mutex);
}

int aloe_sem_wait(aloe_sem_t *ctx, void *rt, long dur_ms, const char *name) {
	int r;

	(void)rt;

	mutex_lock(&ctx->mutex, aloe_dur_infinite);
	while (ctx->cnt == 0) {
		if ((r = cond_wait(&ctx->not_empty, &ctx->mutex,
				aloe_msDur(dur_ms))) != 0) {
			goto finally;
		}
	}
	ctx->cnt--;
	r = 0;
finally:
	pthread_mutex_unlock(&ctx->mutex);
	return r;
}

void aloe_sem_destroy(aloe_sem_t *ctx) {
	pthread_cond_destroy(&ctx->not_empty);
	pthread_mutex_destroy(&ctx->mutex);
}

static void* thread_run(void *_ctx) {
	aloe_thread_t *ctx = (aloe_thread_t*)_ctx;

	(*ctx->run)(ctx);
	return NULL;
}

int aloe_thread_run(aloe_thread_t *ctx, void(*run)(aloe_thread_t*),
		size_t stack, int prio, const char *name) {
#if defined(aloe_thread_name_size) && aloe_thread_name_size > 0
	if (name != ctx->name) {
		snstrcpy(ctx->name, aloe_thread_name_size, name);
	}
#endif
	ctx->run = run;
	return pthread_create(&ctx->thread, NULL, &thread_run, ctx);
}

void* aloe_mem_malloc(aloe_mem_id_t id, size_t sz,
		const char *name __attribute__((unused))) {
	aloe_mem_t *mm = NULL;

	switch (id) {
	case aloe_mem_id_dxmem:
	case aloe_mem_id_psram:
	case aloe_mem_id_stdc:
		mm = (aloe_mem_t*)malloc(sizeof(*mm) + sz);
		break;
	default:
		break;
	}
	if (mm) {
		mm->sig = &aloe_mem_sig;
		mm->id = id;
		mm->sz = sz;
		mm++;
	}
	return (void*)mm;
}

void* aloe_mem_calloc(aloe_mem_id_t id, size_t mb, size_t sz,
		const char *name __attribute__((unused))) {
	aloe_mem_t *mm = NULL;

	sz *= mb;
	switch (id) {
	case aloe_mem_id_dxmem:
	case aloe_mem_id_psram:
	case aloe_mem_id_stdc:
		mm = (aloe_mem_t*)malloc(sizeof(*mm) + sz);
		memset(mm, 0, sizeof(*mm) + sz);
		break;
	default:
		break;
	}
	if (mm) {
		mm->sig = &aloe_mem_sig;
		mm->id = id;
		mm->sz = sz;
		mm++;
	}
	return (void*)mm;
}

int aloe_mem_free(void *_mm) {
	aloe_mem_t *mm;

	if (!_mm) return 0;

	mm = (aloe_mem_t*)_mm - 1;
	if (mm->sig != &aloe_mem_sig) return -1;
	switch (mm->id) {
	case aloe_mem_id_dxmem:
	case aloe_mem_id_stdc:
		free(mm);
		return 0;
	default:
		;
	}
	return -1;
}

#endif
