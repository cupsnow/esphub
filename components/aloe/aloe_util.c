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

#include "aloe_sys.h"

ALOE_SYS_TEXT1_SECTION
aloe_buf_t* aloe_buf_rewind(aloe_buf_t *buf) {
	size_t sz;

	if (buf->pos <= 0) return buf;
	if (buf->pos < buf->lmt) {
		sz = buf->lmt - buf->pos;
		if (sz > 0) memmove(buf->data, (char*)buf->data + buf->pos, sz);
	} else {
		if (buf->lmt > buf->cap || buf->pos > buf->lmt) {
			aloe_log_e("Sanity check invalid %d <= %d <= %d\n",
					buf->pos, buf->lmt, buf->cap);
		}
		sz = 0;
	}
	buf->pos = 0;
	buf->lmt = sz;
	return buf;
}

size_t aloe_buf_add_pos(aloe_buf_t *buf, const void *data, size_t data_sz) {
	size_t sz = buf->lmt - buf->pos;

	if (sz > data_sz) sz = data_sz;
	if (sz > 0) {
		memcpy((char*)buf->data + buf->pos, data, sz);
		buf->pos += sz;
	}
	return sz;
}

size_t aloe_buf_add_lmt(aloe_buf_t *buf, const void *data, size_t data_sz) {
	size_t sz = buf->cap - buf->lmt;

	if (sz > data_sz) sz = data_sz;
	if (sz > 0) {
		memcpy((char*)buf->data + buf->lmt, data, sz);
		buf->lmt += sz;
	}
	return sz;
}

static int _aloe_rb_cmp(aloe_rb_entry_t *a, aloe_rb_entry_t *b) {
	long r;

	if (a->cmp) return (*a->cmp)(a, b);
	r = (long)a - (long)b;
	return r > 0 ? r : r < 0 ? -1 : 0;
}

RB_GENERATE(aloe_rb_tree_rec, aloe_rb_entry_rec, entry, _aloe_rb_cmp);

int aloe_rb_int_cmp(aloe_rb_entry_t *a, aloe_rb_entry_t *b) {
	long r = (long)a->key - (long)b->key;

	return r > 0 ? 1 : r < 0 ? -1 : 0;
}

int aloe_rb_str_cmp(aloe_rb_entry_t *a, aloe_rb_entry_t *b) {
	return strcmp((char*)a->key, (char*)b->key);
}

aloe_rb_entry_t* aloe_rb_find(aloe_rb_tree_t *rb, const void *key,
		aloe_rb_cmp_t cmp) {
	aloe_rb_entry_t ent0;

	ent0.key = key; ent0.cmp = cmp;
	return RB_FIND(aloe_rb_tree_rec, rb, &ent0);
}

size_t aloe_log_vfmsg_def(char *buf, size_t buf_sz, int lvl, const char *tag, long lno,
		const char *fmt, va_list va) {
	int r;
	size_t sz = 0;
	unsigned long ms = aloe_tick2ms(aloe_ticks());

	if ((r = snprintf(buf + sz, buf_sz - sz,
			"[%02lu:%02lu.%03lu]"
			"[%s]"
			"[%s][#%d] ",
			((ms) % 3600000) / 60000, ((ms) % 60000) / 1000, (ms) % 1000,
			aloe_log_level_str2(lvl, "", ""),
			tag, (int)lno)) <= 0 || (sz += r) >= buf_sz) {
		goto finally;
	}
	r = vsnprintf(buf + sz, buf_sz - sz, fmt, va);
	if (r <= 0 || (sz += r) >= buf_sz) goto finally;
finally:
	if (sz >= buf_sz) sz = aloe_strabbr(buf, buf_sz, NULL);
	return sz;
}

size_t aloe_log_vfmsg(char *buf, size_t buf_sz, int lvl, const char *tag,
		long lno, const char*, va_list) __attribute__((weak, alias("aloe_log_vfmsg_def")));;

size_t aloe_log_fmsg(char *buf, size_t buf_sz, int lvl, const char *tag, long lno,
		const char *fmt, ...) {
	va_list va;
	size_t sz;

	va_start(va, fmt);
	sz = aloe_log_vfmsg(buf, buf_sz, lvl, tag, lno, fmt, va);
	va_end(va);
	return sz;
}

void aloe_log_add_va_def(int lvl, const char *tag, long lno,
		const char *fmt, va_list va) {
	char buf[300];
	int sz;

	sz = aloe_log_vfmsg(buf, sizeof(buf), lvl, tag, lno, fmt, va);
	if (sz > 0) printf("%s", buf);
}

void aloe_log_add_va(int lvl, const char *tag, long lno,
		const char*, va_list) __attribute__((weak, alias("aloe_log_add_va_def")));

void aloe_log_add_def(int lvl, const char *tag, long lno,
		const char *fmt, ...) {
	va_list va;

	va_start(va, fmt);
	aloe_log_add_va(lvl, tag, lno, fmt, va);
	va_end(va);
}

void aloe_log_add(int lvl, const char *tag, long lno,
		const char*, ...) __attribute__((weak, alias("aloe_log_add_def")));

int aloe_strabbr(char *buf, int buf_sz, const char *abbr) {
	int abbr_sz;

	// assure abbr is trailing-zero
	if (!abbr) abbr = "..." aloe_endl;
	abbr_sz = strlen(abbr);

	if (abbr_sz && (buf_sz > abbr_sz)) {
		// also copy abbr trailing-zero
		memcpy(buf + buf_sz - 1 - abbr_sz, abbr, abbr_sz + 1);
		// return size excluding trailing-zero
		return buf_sz - 1;
	}
	return 0;
}

ALOE_SYS_TEXT1_SECTION
void aloe_int2hexstr(void *_buf, unsigned val, unsigned width, int cap) {
	char *buf = (char*)_buf;
	int nw;

	if (!buf || width < 1) return;
	buf += width;

	nw = aloe_min(width, sizeof(val) * 2);
	width -= nw;
	while (nw-- > 0) {
		int d = val & 0xf;
		*--buf = _aloe_int2hexstr(d, cap);
		val >>= 4;
	}
	while (width-- > 0) {
		*--buf = '?';
	}
}

ALOE_SYS_TEXT1_SECTION
size_t aloe_hd2(void *_buf, size_t buf_sz, const void *data, size_t data_cnt,
		unsigned width, const char *sep) {
	size_t iu, sep_len;
	uint8_t *buf = (uint8_t*)_buf;
#define width_val(_p, _w) ((_w) == 4 ? *(uint32_t*)(_p) : \
		(_w) == 2 ? *(uint16_t*)(_p) : *(uint8_t*)(_p))

	for (iu = (1 << 2); iu > 0; iu>>=1) {
		if (width >= iu) {
			width = iu;
			break;
		}
	}
	if (width <= 0) width = 1;

	// buf enough for [hex*2 ..., \0]
	if (data_cnt <= 0 || buf_sz < (width * 2 + 1)) return 0;

	if (!sep) sep = " ";
	sep_len = strlen(sep);

	aloe_int2hexstr(buf, width_val(data, width), width * 2, 'a');
	buf += width * 2;
	buf_sz -= width * 2;
	data = (uint8_t*)data + width;

	for (iu = 1; iu < data_cnt; iu++, data = (uint8_t*)data + width,
			buf += (sep_len + width * 2)) {
		// buf enough for [sp, hex*2 ..., \0]
		if (buf_sz < (sep_len + width * 2 + 1)) break;
		memcpy(buf, sep, sep_len);
		aloe_int2hexstr(buf + sep_len, width_val(data, width), width * 2, 'a');
	}
	*buf = '\0';
	return (iu - 1) * sep_len + iu * width * 2;
#undef width_val
}

unsigned aloe_cksum(const void *buf, size_t sz, unsigned cksum) {
	for ( ; sz > 0; sz--, buf = (const void*)((uint8_t*)buf + 1)) {
		cksum += *(uint8_t*)buf;
	}
	return cksum;
}
