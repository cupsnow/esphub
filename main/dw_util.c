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

#include <ctype.h>
#include "dw_util.h"

#define log_d(_fmt...) dw_log_m("[Debug]", _fmt)
#define log_e(_fmt...) dw_log_m("[ERROR]", _fmt)

ALOE_SYS_BSS1_SECTION
static const char *xp = NULL;
ALOE_SYS_TEXT1_SECTION
const char *dw_xp(int var) {
	(void)var;

	if (!xp) xp = ""; // (IPC_CPUID() == 1 ? "HP" : "LP");
	return xp;
}

ALOE_SYS_TEXT1_SECTION
void _dw_dump16(const void *data, size_t sz, const char *func, long lno,
		const char *fmt, ...) {
	va_list va;

	// <str1 | str2>
	char str1[] = {"00 01 02 03 04 05 06 07 08 09 0a 0b 0c 0d 0e 0f 10 11 12"};
	char str2[] = {"0123456789abcdef012"};
	char *s1, *s2;
	int i, osz = sz > 16 ? 16 : sz;

	for (s1 = str1, s2 = str2, i = 0; i < osz; i++) {
		int val = ((char*)data)[i];

		*s1++ = _aloe_int2hexstr((val >> 4) & 0xf, 'a');
		*s1++ = _aloe_int2hexstr((val) & 0xf, 'a');
		*s1++ = ' ';
		*s2++ = isprint(val) ? val : '.';
	}
	str1[osz * 3] = '\0';
	str2[osz] = '\0';

	if (func && fmt) {
		printf("[%d][Debug][%s][%s][#%d]", (unsigned)aloe_tick2ms(aloe_ticks()),
				dw_xp(0), func, (int)lno);
		va_start(va, fmt);
		vprintf(fmt, va);
		va_end(va);
	}
	printf("%d <%s \"%s\">\n", (int)sz, str1, str2);
}
