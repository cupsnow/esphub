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

#ifndef _H_DK_DECKWIFI_HP_DW_SINSVC
#define _H_DK_DECKWIFI_HP_DW_SINSVC

#include <aloe_sys.h>

#include <lwip/sockets.h>

#ifdef __cplusplus
extern "C" {
#endif

int dw_sinsvc2_init(void);
int dw_sinsvc2_send(const void *data, size_t size);
int dw_svcaddr(char *addr, size_t len, struct in_addr *sin_addr);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* _H_DK_DECKWIFI_HP_DW_SINSVC */
