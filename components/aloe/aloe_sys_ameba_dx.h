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

#ifndef _H_ALOE_SYS_AMEBA_DX
#define _H_ALOE_SYS_AMEBA_DX

#ifndef _H_ALOE_SYS
#  error "Please included <aloe/sys.h> instead!"
#endif

#include "aloe_sys_ameba.h"

#ifdef __cplusplus
extern "C" {
#endif

//#define dw_mm_dxmem(_nm, _sz) dw_mm_calloc(dw_mm_id_dxmem, _nm, 1, sz)
//#define dw_mm_psram(_nm, _sz) dw_mm_malloc(dw_mm_id_psram, _nm, sz)

#define ALOE_SYS_TEXT1_SECTION PSRAM_TEXT_SECTION
#define ALOE_SYS_BSS1_SECTION PSRAM_BSS_SECTION
#define ALOE_SYS_DATA1_SECTION PSRAM_DATA_SECTION

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* _H_ALOE_SYS_AMEBA_DX */
