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

#define dw_log_m(_lvl, _fmt, _args...) printf("[%d]%s[%s][%s][#%d]" _fmt, \
		(unsigned)aloe_tick2ms(aloe_ticks()), _lvl, dw_xp(0), __func__, __LINE__, ##_args)

/** Get LP or HP. */
const char *dw_xp(int var);

void _dw_dump16(const void *data, size_t sz, const char *func, long lno,
		const char *fmt, ...);
#define dw_dump16(_data, _sz, _args...) _dw_dump16(_data, _sz, __func__, __LINE__, _args)


#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* _H_DK_DECKWIFI_NP_UTIL */
