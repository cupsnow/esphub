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
#include <dxOS.h>
#include <psram_reserve.h>

/*
#define dw_log_m(_lvl, _fmt, _args...) DiagPrintf("[%d]%s[%s][%s][#%d]" _fmt, \
		(unsigned)aloe_tick2ms(aloe_ticks()), _lvl, dw_xp(0), __func__, __LINE__, ##_args)
#define dw_log_rd(_lvl, _fmt, _args...) DiagPrintf("[%d]%s[%s][%s][#%d]" _fmt, \
		(unsigned)aloe_tick2ms(aloe_ticks()), _lvl, dw_xp(0), __func__, \
		__LINE__, ##_args)
#define dw_log_hp(_lvl, _fmt, _args...) DX_PRINT_RAW(PreChar "[%s:%4d] " _fmt, \
		__func__, __LINE__, ##_args)

#ifdef ALOE_SYS_AMEBA_LP
#  define dw_log_m(...) dw_log_rd(__VA_ARGS__)
#else // ALOE_SYS_AMEBA_LP
#  define dw_log_m(_lvl, _fmt, _args...) DX_PRINT_RAW(PreChar "[%s:%4d]%s " _fmt, \
		__func__, __LINE__, _lvl, ##_args)
#endif // ALOE_SYS_AMEBA_LP
 */

void* aloe_mem_malloc(aloe_mem_id_t id, size_t sz, const char *name) {
	aloe_mem_t *mm = NULL;

	switch (id) {
	case aloe_mem_id_dxmem:
		mm = (aloe_mem_t*)dxMemAlloc((char*)name, 1, sizeof(*mm) + sz);
		break;
	case aloe_mem_id_psram:
		mm = (aloe_mem_t*)Psram_reserve_malloc(sizeof(*mm) + sz);
		break;
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

void* aloe_mem_calloc(aloe_mem_id_t id, size_t mb, size_t sz, const char *name) {
	aloe_mem_t *mm = NULL;

	sz *= mb;
	switch (id) {
	case aloe_mem_id_dxmem:
		mm = (aloe_mem_t*)dxMemAlloc((char*)name, 1, sizeof(*mm) + sz);
		break;
	case aloe_mem_id_psram:
		mm = (aloe_mem_t*)Psram_reserve_malloc(sizeof(*mm) + sz);
		memset(mm, 0, sizeof(*mm) + sz);
		break;
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
		dxMemFree(mm);
		return 0;
	case aloe_mem_id_psram:
		Psram_reserve_free(mm);
		return 0;
	case aloe_mem_id_stdc:
		free(mm);
		return 0;
	default:
		;
	}
	return -1;
}

