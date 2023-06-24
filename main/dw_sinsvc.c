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

#include "dw_sinsvc.h"

#ifdef _H_DK_DECKWIFI_HP_DW_SINSVC

#include <sdkconfig.h>
#include <esp_wifi.h>

#include <lwip/ip_addr.h>
#include <lwip/netdb.h>

#include "dw_util.h"

#define log_d(_fmt...) dw_log_m("[Debug]", _fmt)
#define log_e(_fmt...) dw_log_m("[ERROR]", _fmt)

typedef enum {
#define sel_ent(_nm, _b) \
	sel_ ## _nm ## _bit = _b, \
	sel_ ## _nm = 1 << (_b)

	sel_ent(rd, 0),
	sel_ent(wr, 1),
	sel_ent(ex, 2),
	sel_ent(tmr, 3),

	sel_sockrw = sel_rd | sel_wr,
	sel_sockact = sel_rd | sel_wr | sel_ex,

} sel_t;

typedef struct sock_rec {
	int fd;
	unsigned sel_req;
	unsigned long tdue;
	struct sockaddr_in sin;
	void (*act)(struct sock_rec*, unsigned sel_res);
	struct sock_rec *next;
} sock_t;

typedef struct {
	sock_t sock;
	unsigned long ts0;
	aloe_buf_t fb;

	char buf[64]; // buf afterward data not memset(0)
	int id;
} client_t;

typedef struct {
	sock_t sock;
} svc_t;

typedef struct {
	unsigned connected: 1;
	sock_t sock;
} mgmt_t;

ALOE_SYS_DATA1_SECTION
static struct {
	unsigned ready: 1;
	unsigned launched: 1;
	unsigned quit: 1;
	svc_t svc;
	mgmt_t mgmt;
	client_t cln[2];
	aloe_thread_t tsk;
	int fds_mx;
	fd_set fds_rd, fds_wr, fds_ex;

	// shared buffer between within sinsvc_task()
	struct sockaddr_in sin;

	char buf[1024]; // buf afterward data not memset(0)
} impl = {};

/** Close fd and set to -1. */
#define fd_gc(_fd) do { close(_fd); _fd = -1; } while(0)

/** Set Nonblock. */
#define fd_nbio(_fd) fcntl(_fd, F_SETFL, fcntl(_fd, F_GETFL, 0) | O_NONBLOCK)

/** Set due time, for timeout. */
#define sock_tdue(_dur) ((unsigned long)aloe_tick2ms(aloe_ticks()) + (_dur))

/** Prepare FD_SET and max fd number for select(). */
#define sinsvc_fds(_sock) do { \
	if (impl.fds_mx < (_sock)->fd) impl.fds_mx = (_sock)->fd; \
	if ((_sock)->sel_req & sel_rd) FD_SET((_sock)->fd, &impl.fds_rd); \
	if ((_sock)->sel_req & sel_wr) FD_SET((_sock)->fd, &impl.fds_wr); \
	if ((_sock)->sel_req & sel_ex) FD_SET((_sock)->fd, &impl.fds_ex); \
} while(0)

/** Prepare min due time for select(). */
#define sinsvc_tdue(_tdue, _ts0, _sock_tdue) if ( \
		((_tdue) == aloe_dur_infinite || (_tdue) > (_ts0))) { \
	if ((_sock_tdue) == 0) { \
		(_tdue) = (_ts0); \
	} else if (((_sock_tdue) != aloe_dur_infinite) \
			&& ((_tdue) > (_sock_tdue))) { \
		(_tdue) = (_sock_tdue); \
	} \
}

/** Check FD_ISSET. */
#define sinsvc_sel(_sel, _sock) do { \
		if (FD_ISSET((_sock)->fd, &impl.fds_rd)) *(_sel) |= sel_rd; \
		if (FD_ISSET((_sock)->fd, &impl.fds_wr)) *(_sel) |= sel_wr; \
		if (FD_ISSET((_sock)->fd, &impl.fds_ex)) *(_sel) |= sel_ex; \
} while(0)

/** Check timeout. */
#define sinsvc_ts_tdue(_ts1, _sock_tdue) ( \
		(_sock_tdue) != aloe_dur_infinite \
		&& (_ts1) >= (_sock_tdue))

static int sinsvc_close_all(void) {
	if (impl.svc.sock.fd != -1) {
		fd_gc(impl.svc.sock.fd);
	}
	for (int i = 0; i < (int)aloe_arraysize(impl.cln); i++) {
		client_t *cln = &impl.cln[i];

		if (cln->sock.fd != -1) {
			fd_gc(cln->sock.fd);
		}
	}
	impl.launched = 0;
	return 0;
}

static void cln_act(struct sock_rec *_sock, unsigned actype) {
//	client_t *cln = aloe_container_of(_sock, client_t, sock);
	int r = 0;

	if (actype & sel_rd) {
		r = read(_sock->fd, impl.buf, sizeof(impl.buf));
		if (r == 0) {
			r = -1;
#if 1
			if (inet_ntop(AF_INET, &_sock->sin.sin_addr, impl.buf, sizeof(impl.buf))) {
				log_d("remote closed %s:%d\n", impl.buf,
						ntohs(_sock->sin.sin_port));
			} else
#endif
			{
				log_d("remote closed\n");
			}
			goto finally;
		}
		if (r < 0) {
			r = errno;
			if (r == EAGAIN
#ifdef EWOULDBLOCK
					|| r == EWOULDBLOCK
#endif
					) {
				r = 0;
				goto finally;
			}
		}
		dw_dump16(impl.buf, r, "recv: ");
	}

	if (actype & sel_wr) {
		r = 0;
	}

	if (actype & sel_sockrw) {
		goto finally;
	}

	if (actype & sel_tmr) {
#if 1
		if (inet_ntop(AF_INET, &_sock->sin.sin_addr, impl.buf, sizeof(impl.buf))) {
			log_d("client timeout %s:%d\n", impl.buf,
					ntohs(_sock->sin.sin_port));
		} else
#endif
		{
			log_d("client timeout\n");
		}
		goto finally;
	}

finally:
	if (r < 0) {
		fd_gc(_sock->fd);
	} else {
		_sock->sel_req = sel_rd;
		_sock->tdue = sock_tdue(10000);
	}
}

static void svc_act(struct sock_rec *_sock, unsigned actype) {
	svc_t *svc = NULL;
	int fd = -1, i;

	if ((_sock != &impl.svc.sock)) {
		log_e("Sanity check invalid svc\n");
		return;
	}
	svc = &impl.svc;

	if (actype & sel_rd) {
		client_t *cln = NULL;
		struct sockaddr_in *sin = &impl.sin;
		socklen_t sin_len = sizeof(*sin);

		if ((fd = accept(_sock->fd, (struct sockaddr*)sin, &sin_len)) == -1) {
			log_e("Failed accept\n");
			goto finally;
		}
		if (!inet_ntop(AF_INET, &sin->sin_addr, impl.buf, sizeof(impl.buf))) {
			log_e("invalid address\n");
			goto finally;
		}
		log_d("remote address: %s:%d\n", impl.buf, ntohs(sin->sin_port));

		if (fd_nbio(fd) != 0) {
			log_e("Failed set nonblock\n");
			goto finally;
		}

		for (i = 0; i < (int)aloe_arraysize(impl.cln); i++) {
			if (impl.cln[i].sock.fd == -1) break;
		}
		if (i >= (int)aloe_arraysize(impl.cln)) {
			log_e("all client slot busy\n");
			goto finally;
		}
		cln = &impl.cln[i];
		memset(cln, 0, offsetof(client_t, buf));
//		cln->sock.fd = -1;
		cln->sock.fd = fd;
		cln->sock.sin = *sin;
		cln->sock.sel_req = sel_rd;
		cln->sock.tdue = sock_tdue(5000);
		cln->sock.act = &cln_act;
		fd = -1;
	}

finally:
	if (fd != -1) close(fd);
	if (svc) {
		svc->sock.sel_req = sel_rd;
		svc->sock.tdue = aloe_dur_infinite;
	}
}

static int svc_open(void) {
	svc_t *svc = &impl.svc;

	if (svc->sock.fd != -1) return 0;

	memset(svc, 0, sizeof(*svc));
//	svc->sock.fd = -1;
	if((svc->sock.fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		log_e("create socket\n");
		return -1;
	}

	if (fd_nbio(svc->sock.fd) != 0) {
		log_e("Failed set nonblock\n");
		fd_gc(svc->sock.fd);
		return -1;
	}
	svc->sock.sin.sin_family = AF_INET;
	svc->sock.sin.sin_port = htons(eh_sinsvc_port);
	svc->sock.sin.sin_addr.s_addr = INADDR_ANY;

	if (bind(svc->sock.fd, (struct sockaddr*)&svc->sock.sin,
			sizeof(svc->sock.sin)) != 0) {
		log_e("bind error\n");
		fd_gc(svc->sock.fd);
		return -1;
	}

	if(listen(svc->sock.fd, 2) != 0) {
		log_e("listen error\n");
		fd_gc(svc->sock.fd);
		return -1;
	}

	svc->sock.sel_req = sel_rd;
	svc->sock.tdue = aloe_dur_infinite;
	svc->sock.act = &svc_act;

	if (!inet_ntop(AF_INET, &svc->sock.sin.sin_addr, impl.buf, sizeof(impl.buf))) {
		log_e("invalid address\n");
	} else {
		log_d("listen address: %s:%d\n", impl.buf,
				ntohs(svc->sock.sin.sin_port));
	}
	return 0;
}

static void sinsvc_task(aloe_thread_t *args) {
	unsigned long ts0, ts1, tdue;
	struct timeval _tv, *tv;
	sock_t *sock;
	int r;

	(void)args;

	while (!impl.quit) {
		if (!impl.launched) {
			sinsvc_close_all();
			if (dw_svcaddr(impl.buf, sizeof(impl.buf), NULL) != 0) {
				aloe_thread_sleep(1000);
				continue;
			}
			log_d("ipaddr ready: %s\n", impl.buf);
		}
		if (svc_open() != 0) {
			aloe_thread_sleep(1000);
			continue;
		}
		impl.launched = 1;

		FD_ZERO(&impl.fds_rd);
		FD_ZERO(&impl.fds_wr);
		FD_ZERO(&impl.fds_ex);
		impl.fds_mx = -1;
		ts0 = aloe_tick2ms(aloe_ticks());
		tdue = ts0 + 5000; // todo use mgmt tdue or aloe_dur_infinite;

		if (impl.svc.sock.fd != -1) {
			sock = &impl.svc.sock;

			sinsvc_fds(sock);
			sinsvc_tdue(tdue, ts0, sock->tdue);
		}
		for (int i = 0; i < (int)aloe_arraysize(impl.cln); i++) {
			sock = &impl.cln[i].sock;

			if (sock->fd == -1) continue;

			sinsvc_fds(sock);
			sinsvc_tdue(tdue, ts0, sock->tdue);
		}

		if (impl.fds_mx == -1) {
			// no socket avaliable
			aloe_thread_sleep(1000);
			continue;
		}

		if (tdue == aloe_dur_infinite) {
			tv = NULL;
		} else if (tdue <= ts0) {
			tv = &_tv;
			tv->tv_sec = 0;
			tv->tv_usec = 0;
		} else {
			tv = &_tv;
			tv->tv_sec = (tdue - ts0) / 1000; // ms to sec
			tv->tv_usec = ((tdue - ts0) % 1000) * 1000; // ms to us
		}

		if ((r = select(impl.fds_mx + 1, &impl.fds_rd, &impl.fds_wr,
						&impl.fds_ex, tv)) < 0) {
			r = errno;
			if (r == EINTR) {
//				log_d("eintr\n");
			} else {
				log_e("select failed\n");
				impl.launched = 1;
			}
			aloe_thread_sleep(5000);
			continue;
		}
		if (r == 0) {
			// no act
			continue;
		}

		ts1 = aloe_tick2ms(aloe_ticks());

		if (impl.svc.sock.fd != -1) {
			unsigned sel = 0;

			sock = &impl.svc.sock;

			sinsvc_sel(&sel, sock);

			// timeout iff no sock action
			if (sel == 0 && sinsvc_ts_tdue(ts1, sock->tdue)) {
				sel |= sel_tmr;
			}

			if (sel && sock->act) {
				sock->sel_req = 0;
				sock->tdue = aloe_dur_infinite;
				(*sock->act)(sock, sel);
			}
		}
		for (int i = 0; i < (int)aloe_arraysize(impl.cln); i++) {
			unsigned sel = 0;

			sock = &impl.cln[i].sock;
			if (sock->fd == -1) continue;

			sinsvc_sel(&sel, sock);

			// timeout iff no sock action
			if (sel == 0 && sinsvc_ts_tdue(ts1, sock->tdue)) {
				sel |= sel_tmr;
			}

			if (sel && sock->act) {
				sock->sel_req = 0;
				sock->tdue = aloe_dur_infinite;
				(*sock->act)(sock, sel);
			}
		}

	}
}

ALOE_SYS_TEXT1_SECTION
int dw_sinsvc_init(void) {
	memset(&impl, 0, offsetof(typeof(impl), buf));
	impl.mgmt.sock.fd = -1;
	impl.svc.sock.fd = -1;
	for (int i = 0; i < (int)aloe_arraysize(impl.cln); i++) {
		impl.cln[i].sock.fd = -1;
		impl.cln[i].id = i;
	}

	if (aloe_thread_run(&impl.tsk, &sinsvc_task,
			4096, eh_task_prio1, "sinsvc") != 0) {
		log_e("Failed start sinsvc thread\n");
		return -1;
	}
	impl.ready = 1;
	return 0;
}

ALOE_SYS_TEXT1_SECTION
int dw_sinsvc_launched(void) {
	return impl.launched;
}

ALOE_SYS_TEXT1_SECTION
int dw_svcaddr(char *addr, size_t len, struct in_addr *sin_addr) {
	char _addr[40];
	uint8_t IPv4[4] = {};
	int addrLen;

#if 1
	esp_netif_ip_info_t ipinfo;

	if (esp_netif_get_ip_info(esp_netif_get_default_netif(), &ipinfo) != ESP_OK) {
		log_e("Failed get ip info\n");
		return -1;
	}
	IPv4[0] = ESPIPADDR_ENT(&ipinfo.ip, 0);
	IPv4[1] = ESPIPADDR_ENT(&ipinfo.ip, 1);
	IPv4[2] = ESPIPADDR_ENT(&ipinfo.ip, 2);
	IPv4[3] = ESPIPADDR_ENT(&ipinfo.ip, 3);
#else
	memcpy(IPv4, LwIP_GetIP(&xnetif[0]), sizeof(IPv4));
#endif
	if (IPv4[0] == 0) return -1;

	snprintf(_addr, sizeof(_addr), "%d.%d.%d.%d", IPv4[0], IPv4[1], IPv4[2], IPv4[3]);
	_addr[sizeof(_addr) - 1] = '\0';
	addrLen = strlen(_addr);
	if (addr && len > 0) {
		if (len > (size_t)addrLen) {
			memcpy(addr, _addr, addrLen);
			addr[addrLen] = '\0';
		} else {
			addr[0] = '\0';
		}
	}
	if (sin_addr && !inet_pton(AF_INET, _addr, sin_addr)) {
		memset(sin_addr, 0, sizeof(*sin_addr));
	}
	return 0;
}

#endif
