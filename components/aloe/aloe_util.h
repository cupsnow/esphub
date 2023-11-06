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

#ifndef _H_ALOE_UTIL
#define _H_ALOE_UTIL

#include "aloe_sys.h"
#include "aloe_compat/queue.h"
#include "aloe_compat/tree.h"
#include <stdarg.h>
#include <stddef.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

#define aloe_arraysize(_a) (sizeof(_a) / sizeof((_a)[0]))
#define aloe_min(_a, _b) ((_a) <= (_b) ? (_a) : (_b))
#define aloe_max(_a, _b) ((_a) >= (_b) ? (_a) : (_b))
#define aloe_abs(_v) (((_v) < 0.0) ? (0.0 - (_v)) : (_v))
#define aloe_10e3 1000ul
#define aloe_10e6 1000000ul
#define aloe_10e9 1000000000ul
#define aloe_2e10 1024ul
#define aloe_2e20 1048576ul
#define aloe_2e30 1073741824ul
#define aloe_trex "🦖"
#define aloe_sauropod "🦕"
#define aloe_lizard "🦎"
#define aloe_endl_msw "\r\n"
#define aloe_endl_unix "\n"

#define aloe_stringify(_s) # _s
#define aloe_stringify2(_s) aloe_stringify(_s)
#define aloe_container_of(_obj, _type, _member) \
	((_type *)((_obj) ? ((char*)(_obj) - offsetof(_type, _member)) : NULL))
#define aloe_member_of(_type, _member) ((_type *)NULL)->_member
#define aloe_sizewith(_type, _member) (offsetof(_type, _member) + sizeof(aloe_member_of(_type, _member)))

#define aloe_concat(_s1, _s2) _s1 ## _s2
#define aloe_concat2(_s1, _s2) aloe_concat(_s1, _s2)

#define aloe_bitmask(_bit, _bits) (((1 << (_bits)) - 1) << (_bit))
#define aloe_bitval(_v, _bit, _bits) ((_v) & aloe_bitmask(_bit, _bits))
#define aloe_bitclr(_v, _bit, _bits) ((_v) & ~aloe_bitmask(_bit, _bits))
#define aloe_bitset(_v, _v2, _bit, _bits) (aloe_bitclr(_v, _bit, _bits) | \
		aloe_bitval((_v2) << (_bit), _bit, _bits))
#define aloe_bitval2(_v, _bit, _bits) (((_v) >> (_bit)) & ((1 << (_bits)) - 1))
#define aloe_roundup(_v, _p) (((_v) & ((_p) - 1)) ? (((_v) + (_p)) & ~((_p) - 1)) : (_v))

/** Generate data array.
 *
 * Example:
 *
 * $ hexdump -C gru/profile1.txt
 * 00000000  70 72 6f 66 69 6c 65 31  0a 63 6d 64 31 3d 61 62  |profile1.cmd1=ab|
 * 00000010  63 20 64 65 66 20 67 68  0a 23 20 63 6f 6d 6d 65  |c def gh.# comme|
 * 00000020  6e 74 0a 63 6d 64 32 3d  74 68 69 73 20 69 73 20  |nt.cmd2=this is |
 * 00000030  63 6d 64 32 0a                                    |cmd2.|
 * 00000035
 *
 * $ xxd -i gru/profile1.txt
 * unsigned char profile1_txt[] = {
 *   0x70, 0x72, 0x6f, 0x66, 0x69, 0x6c, 0x65, 0x31, 0x0a, 0x63, 0x6d, 0x64,
 *   0x31, 0x3d, 0x61, 0x62, 0x63, 0x20, 0x64, 0x65, 0x66, 0x20, 0x67, 0x68,
 *   0x0a, 0x23, 0x20, 0x63, 0x6f, 0x6d, 0x6d, 0x65, 0x6e, 0x74, 0x0a, 0x63,
 *   0x6d, 0x64, 0x32, 0x3d, 0x74, 0x68, 0x69, 0x73, 0x20, 0x69, 0x73, 0x20,
 *   0x63, 0x6d, 0x64, 0x32, 0x0a
 * };
 * unsigned int profile1_txt_len = 53;
 *
 */
#define aloe_xxd_gen(_n) \
	extern unsigned char * _n; \
	extern unsigned int _n ## _len;

/** Define bit mask against enumeration.
 *
 *   Define group of bit masked flag in the enumeration.  The group of flag
 * occupied \<ITEM\>_mask_offset (count from bit0), and take \<ITEM\>_mask_bits.
 * \<ITEM\>_mask used to filter the occupied value.
 *
 * Example:
 * @code{.c}
 * typedef enum flag_enum {
 *   ALOE_FLAG_MASK(flag_class, 0, 2),
 *   ALOE_FLAG(flag_class, _case, 0),
 *   ALOE_FLAG(flag_class, _suite, 1),
 *   ALOE_FLAG_MASK(flag_result, 2, 2),
 *   ALOE_FLAG(flag_result, _pass, 0),
 *   ALOE_FLAG(flag_result, _failed_memory, 1),
 *   ALOE_FLAG(flag_result, _failed_io, 2),
 * } flag_t;
 *
 * flag_t flags = flag_class_suite | flag_result_failed_io; // 0x9
 * flag_t flags_class = flags & flag_class_mask; // 0x1
 * flag_t flags_result = flags & flag_result_mask; // 0x8
 * @endcode
 */
#define ALOE_FLAG_MASK(_group, _offset, _bits) \
		_group ## _mask_offset = _offset, \
		_group ## _mask_bits = _bits, \
		_group ## _mask = (((1 << (_bits)) - 1) << (_offset))

/** Define bit masked value against enumeration.
 *
 * Reference to ALOE_FLAG_MASK()
 */
#define ALOE_FLAG(_group, _name, _val) \
	_group ## _name = ((_val) << _group ## _mask_offset)

/** Generic buffer holder. */
typedef struct aloe_buf_rec {
	void *data; /**< Memory pointer. */
	size_t cap; /**< Memory capacity. */
	size_t lmt; /**< Data size. */
	size_t pos; /**< Data start. */
} aloe_buf_t;

#define _aloe_buf_clear(_buf) do {(_buf)->lmt = (_buf)->cap; (_buf)->pos = 0;} while (0)
#define _aloe_buf_flip(_buf) do {(_buf)->lmt = (_buf)->pos; (_buf)->pos = 0;} while (0)

aloe_buf_t* aloe_buf_clear(aloe_buf_t *buf);
aloe_buf_t* aloe_buf_flip(aloe_buf_t *buf);
aloe_buf_t* aloe_buf_rewind(aloe_buf_t *buf);

size_t aloe_rinbuf_read(aloe_buf_t *buf, void *data, size_t sz);
size_t aloe_rinbuf_write(aloe_buf_t *buf, const void *data, size_t sz);
size_t aloe_buf_add_pos(aloe_buf_t *buf, const void*, size_t);
size_t aloe_buf_add_lmt(aloe_buf_t *buf, const void*, size_t);

#define aloe_rinbuf1_entry(_nm, _sz) struct _nm { \
	char data[_sz]; \
	volatile unsigned rd, wr; \
}

#define aloe_rinbuf1_empty(_rinbuf) ((_rinbuf)->wr == (_rinbuf)->rd)

// rinbuf data size must be power of 2
#define aloe_rinbuf1_m(_rinbuf) (aloe_arraysize((_rinbuf)->data) - 1)
#define aloe_rinbuf1_data_len2(_rinbuf) ((_rinbuf)->wr - (_rinbuf)->rd)
#define aloe_rinbuf1_full2(_rinbuf) (aloe_rinbuf1_data_len2(_rinbuf) == aloe_arraysize((_rinbuf)->data))
#define aloe_rinbuf1_putc2(_rinbuf, _b) do { \
	(_rinbuf)->data[(_rinbuf)->wr & aloe_rinbuf1_m(_rinbuf)] = (_b); \
	(_rinbuf)->wr++; \
} while(0);
#define aloe_rinbuf1_getc2(_rinbuf, _b) do { \
	(_b) = (_rinbuf)->data[(_rinbuf)->rd & aloe_rinbuf1_m(_rinbuf)]; \
	(_rinbuf)->rd++; \
} while(0);

/** Entry to tail queue.
 *
 * @param entry
 */
typedef struct aloe_tailq_entry_rec {
	const void *key;
	TAILQ_ENTRY(aloe_tailq_entry_rec) entry;
} aloe_tailq_entry_t;

/** Head to tail queue. */
typedef TAILQ_HEAD(aloe_tailq_rec, aloe_tailq_entry_rec) aloe_tailq_t;

typedef struct aloe_rb_entry_rec aloe_rb_entry_t;

typedef int (*aloe_rb_cmp_t)(aloe_rb_entry_t*, aloe_rb_entry_t*);

/** Entry to red-black tree. */
struct aloe_rb_entry_rec {
	const void *key;
	aloe_rb_cmp_t cmp;
	RB_ENTRY(aloe_rb_entry_rec) entry;
};

/** Head to red-black tree. */
typedef RB_HEAD(aloe_rb_tree_rec, aloe_rb_entry_rec) aloe_rb_tree_t;

RB_PROTOTYPE(aloe_rb_tree_rec, aloe_rb_entry_rec, entry, );

aloe_rb_entry_t* aloe_rb_find(aloe_rb_tree_t*, const void*, aloe_rb_cmp_t);
#define aloe_rb_next(_rb, _prev) ( \
		(_prev) ? RB_NEXT(aloe_rb_tree_rec, _rb, _prev) : \
				RB_MIN(aloe_rb_tree_rec, _rb))

int aloe_rb_int_cmp(aloe_rb_entry_t*, aloe_rb_entry_t*);
#define aloe_rb_int_insert(_rb, _ent) do { \
	(_ent)->cmp = &aloe_rb_int_cmp; \
	RB_INSERT(aloe_rb_tree_rec, _rb, _ent); \
} while(0)
#define aloe_rb_int_find(_rb, _k) aloe_rb_find(_rb, _k, &aloe_rb_int_cmp)

int aloe_rb_str_cmp(aloe_rb_entry_t*, aloe_rb_entry_t*);
#define aloe_rb_str_insert(_rb, _ent) do { \
	(_ent)->cmp = &aloe_rb_str_cmp; \
	RB_INSERT(aloe_rb_tree_rec, _rb, _ent); \
} while(0)
#define aloe_rb_str_find(_rb, _k) aloe_rb_find(_rb, _k, &aloe_rb_str_cmp)

/** @defgroup ALOE_LOG Debug message
 * @ingroup ALOE
 *
 * @{
 */
#define aloe_log_d(...) aloe_log_add(aloe_log_level_debug, __func__, __LINE__, __VA_ARGS__)
#define aloe_log_e(...) aloe_log_add(aloe_log_level_error, __func__, __LINE__, __VA_ARGS__)

size_t aloe_log_vfmsg_def(char *buf, size_t buf_sz, int lvl,
		const char *tag, long lno, const char*, va_list);
size_t aloe_log_vfmsg(char *buf, size_t buf_sz, int lvl, const char *tag,
		long lno, const char*, va_list);
size_t aloe_log_fmsg(char *buf, size_t buf_sz, int lvl, const char *tag,
		long lno, const char*, ...);

void aloe_log_add_va_def(int lvl, const char *tag, long lno, const char*,
		va_list);
void aloe_log_add_va(int lvl, const char *tag, long lno, const char*,
		va_list);
void aloe_log_add_def(int lvl, const char *tag, long lno, const char*,
		...);
void aloe_log_add(int lvl, const char *tag, long lno, const char*, ...);

/** Patch the abbreviate string to the end of buffer.
 *
 * Useful for logger.
 *
 * @param buf
 * @param buf_sz
 * @param abbr The abbreviate string.
 * @return (buf_sz - 1) if the abbr patched, otherwise 0
 */
int aloe_strabbr(char *buf, int buf_sz, const char *abbr);

typedef enum aloe_log_level_enum {
	ALOE_FLAG_MASK(aloe_log_level, 0, 4),
	ALOE_FLAG(aloe_log_level, _error, 1),
	ALOE_FLAG(aloe_log_level, _info, 2),
	ALOE_FLAG(aloe_log_level, _debug, 3),
	ALOE_FLAG(aloe_log_level, _verbose, 4),
} aloe_log_level_t;

#define aloe_log_level_str2(_lvl, _na, _lvl_tr) ( \
	((_lvl) & aloe_log_level_mask) == aloe_log_level_error ? "ERROR" _lvl_tr : \
	((_lvl) & aloe_log_level_mask) == aloe_log_level_info ? "INFO" _lvl_tr : \
	((_lvl) & aloe_log_level_mask) == aloe_log_level_debug ? "Debug" _lvl_tr : \
	((_lvl) & aloe_log_level_mask) == aloe_log_level_verbose ? "verbose" _lvl_tr : \
	(_na))

/** @} ALOE_LOG */

#define _aloe_int2hexstr(_d, _a) ((_d) < 10 ? (_d) + '0' : (_d) - 10 + (_a))

void aloe_int2hexstr(void *_buf, unsigned val, unsigned width, int cap);

size_t aloe_hd2(void *_buf, size_t buf_sz, const void *data, size_t data_cnt,
		unsigned width, const char *sep);

#define aloe_hd(_arr, _v, _s) aloe_hd2(_arr, sizeof(_arr), _v, _s, 1, NULL)

unsigned aloe_cksum(const void *buf, size_t sz, unsigned cksum);

typedef struct {
	volatile unsigned wr_cnt, rd_cnt, fb_cnt, wr_idx;
	aloe_buf_t *fb;
} aloe_rinfb_t;

int aloe_rinfb_init(aloe_rinfb_t *rinfb, void *buf, int rinfb_unit, int rinfb_cnt);

#define aloe_rinfb_full(_rinfb) ((_rinfb)->wr_cnt - (_rinfb)->rd_cnt == (_rinfb)->fb_cnt)
#define aloe_rinfb_empty(_rinfb) ((_rinfb)->wr_cnt - (_rinfb)->rd_cnt == 0)
#define aloe_rinfb_wr_idx(_rinfb) ((_rinfb)->wr_cnt % (_rinfb)->fb_cnt)
#define aloe_rinfb_rd_idx(_rinfb) ((_rinfb)->rd_cnt % (_rinfb)->fb_cnt)

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* _H_ALOE_UTIL */
