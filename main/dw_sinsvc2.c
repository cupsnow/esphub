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

#include <aloe_unitest.h>

#include <fcntl.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <net/if.h>

#include <esp_wifi.h>
#include "dw_util.h"
#include "dw_spi.h"

#define log_d(...) aloe_log_d(__VA_ARGS__)
#define log_e(...) aloe_log_e(__VA_ARGS__)

// receive without block
//#define TEST_SA6138_WIFI_PBUF 1

// forward/extern declaration

typedef struct __attribute__((packed)) {
	uint32_t tag;

	/**
	 * len = bytes count after field len
	 * maybe useful aloe_sizewith(dw_pkt1_t, len)
	 */
	uint32_t len;

	uint8_t pld[0];
} dw_pkt2_t;

typedef enum {
#define tag_ent(_nm, _b) \
	sinsvc2_pkt2_tag_ ## _nm ## _bit = _b, \
	sinsvc2_pkt2_tag_ ## _nm = 1 << (_b)

	tag_ent(s, 0),
	tag_ent(e, 1),

#undef tag_ent
} sinsvc2_pkt2_tag_t;

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
#undef sel_ent
} sel_t;

typedef struct sock_rec {
	int fd;
	unsigned sel_req;
	unsigned long tdue;
	struct sockaddr_in sin;
	void (*act)(struct sock_rec*, unsigned sel_res);
} sock_t;

typedef struct {
	sock_t sock;
} svc_t;

#define frm_req_sz (4 * 1024)
typedef struct {
	dw_spi2_req_t spi2_req;
	uint16_t flag;
	aloe_buf_t fb;
} frm_req_t;

typedef struct {
	sock_t sock;

	/* hold dw_pkt1_t in data field */
	aloe_buf_t recv, resp;

	frm_req_t *frm;
	dw_pkt2_t pkthdr;
	size_t pkt_lmt;

	struct {
		unsigned long ts_accept, ts_log, acc;
		unsigned long ts_log_frm, acc_frm_spi;
	} st;

} cln_t;

typedef struct {
	sock_t sock;

	/* 32 bytes alignment */
#define mgmt_sz (2 * 1024)

	/* outward data feed to mgmt, then copy to client */
	aloe_buf_t store;
	aloe_sem_t store_lock;

} mgmt_t;

ALOE_SYS_DATA1_SECTION
static struct {
	unsigned quit: 1;
	unsigned launched: 1;
	unsigned ready: 1;

	aloe_thread_t tsk;

	/* for select() */
	int fds_mx;
	fd_set fds_rd, fds_wr, fds_ex;

	mgmt_t mgmt;

	svc_t svc;

#define cln_cnt 1

	/* 32 bytes alignment */
#define cln_recv_sz (64 * 1024)
#define cln_resp_sz (128)

	cln_t cln[cln_cnt];

	void *xfer, *xfer_alloc;

#define sock_cnt (1 + 1 + cln_cnt)
	sock_t *sock_list[sock_cnt];

#define frm_req_cnt ((int)((15 * 1024) / frm_req_sz))
	dw_spi2_req_list_t frm_list;
	aloe_sem_t frm_lock;

} impl = {};

static const size_t pkt2_hdr_len = aloe_sizewith(dw_pkt2_t, len);

/** Close fd and set to -1. */
#define fd_gc(_fd) do { close(_fd); _fd = -1; } while(0)

/** Set Nonblock. */
#define fd_nbio(_fd) fcntl(_fd, F_SETFL, fcntl(_fd, F_GETFL, 0) | O_NONBLOCK)

/* ie. for connect() */
#ifdef EINPROGRESS
#  define eno_inprogress(_eno) (((_eno) == EAGAIN) || ((_eno) == EINPROGRESS))
#else
#  define eno_inprogress(_eno) ((_eno) == EAGAIN)
#endif

/* ie. for read() or write() */
#ifdef EWOULDBLOCK
#  define eno_wouldblock(_eno) (((_eno) == EAGAIN) || ((_eno) == EWOULDBLOCK))
#else
#  define eno_wouldblock(_eno) ((_eno) == EAGAIN)
#endif

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

#define sinsvc_cln_reset(_cln) do { \
		(_cln)->sock.fd = -1; \
		_aloe_buf_clear(&(_cln)->recv); \
		(_cln)->resp.pos = (_cln)->resp.lmt = 0; \
		(_cln)->frm = NULL; \
		(_cln)->pkt_lmt = 0; \
} while(0)

#define log_sockaddr(_msg, _sin) do { \
	char _buf[50]; \
	if (!inet_ntop(AF_INET, &(_sin)->sin_addr, _buf, sizeof(_buf))) { \
		log_e(_msg "invalid address\n"); \
	} else { \
		log_d(_msg "%s:%d\n", _buf, ntohs((_sin)->sin_port)); \
	} \
} while(0)

// caution for conflict variables
#  define FPS_CALC1(_acc, _tdur) do { \
	r2 = (double)(_acc) / (_tdur); \
	rd100 = (r2 - (int)r2) * 100.0; \
} while(0)

ALOE_SYS_TEXT1_SECTION
static int sinsvc2_svcaddr(char *addr, size_t len, struct in_addr *sin_addr) {
	char _addr[40];
	esp_netif_ip_info_t ipinfo;
	int addrLen;

	if (esp_netif_get_ip_info(esp_netif_get_default_netif(), &ipinfo) != ESP_OK) {
//		log_e("Failed get ip info\n");
		return -1;
	}

	log_d("wifi ready ip %d.%d.%d.%d, netmask %d.%d.%d.%d, gw: %d.%d.%d.%d\n",
			ESPIPADDR_PKARG(&ipinfo.ip), ESPIPADDR_PKARG(&ipinfo.netmask),
			ESPIPADDR_PKARG(&ipinfo.gw));

	if (ESPIPADDR_ENT(&ipinfo.ip, 0) == 0) return -1;

	if ((addr && len > 0) || sin_addr) {
		snprintf(_addr, sizeof(_addr), "%d.%d.%d.%d",
				ESPIPADDR_PKARG(&ipinfo.ip));
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
	}
	return 0;
}

#if 0
static int mgmt_pipe_close(void) {
	mgmt_t *mgmt = &impl.mgmt;

	if (mgmt->sock.fd != -1) {
		fd_gc(mgmt->sock.fd);
		fd_gc(mgmt->pipe_fd1);
	}
}

static int mgmt_pipe_kick(void) {
	mgmt_t *mgmt = &impl.mgmt;
	int r = 1;

	if (mgmt->sock.fd != -1
			&& write(impl.mgmt.pipe_fd1, &r, 1) != 1) {
		return -1;
	}
	return 0;
}

static int mgmt_pipe_open(void (*act)(struct sock_rec*, unsigned),
		unsigned long tdur) {
	mgmt_t *mgmt = &impl.mgmt;
	int r, pfd[2] = {-1, -1};

	if (pipe(pfd) != 0) {
		r = errno;
		log_e("open pipe: %s\n", strerror(r));
		return -1;
	}

	if ((act || tdur != aloe_dur_infinite) && fd_nbio(pfd[0]) != 0) {
		log_e("Failed set nonblock\n");
		close(pfd[0]); close(pfd[1]);
		return -1;
	}
	if (act) {
		mgmt->sock.sel_req = sel_rd;
		mgmt->sock.tdue = (tdur == aloe_dur_infinite) ? tdur : sock_tdue(tdur);
		mgmt->sock.act = act;
	}

	mgmt->sock.fd = pfd[0];
	mgmt->pipe_fd1 = pfd[1];
	return 0;
}
#endif

static int sinsvc_close_all(void) {
	int i;

	for (i = 0; i < sock_cnt; i++) {
		if (impl.sock_list[i]->fd != -1) {
			fd_gc(impl.sock_list[i]->fd);
		}
	}
	impl.launched = 0;
	return 0;
}

static int sock_svr_open(sock_t *sock, uint16_t port, int sockType,
		void (*act)(struct sock_rec*, unsigned), unsigned long tdur) {
	int opt1;

	if((sock->fd = socket(AF_INET, sockType, 0)) == -1) {
		log_e("create socket\n");
		return -1;
	}

	opt1 = 1;
    if (setsockopt(sock->fd, SOL_SOCKET, SO_REUSEADDR, &opt1, sizeof(opt1)) != 0) {
    	log_e("Failed set SO_REUSEADDR\n");
    }

	if ((act || tdur != aloe_dur_infinite) && fd_nbio(sock->fd) != 0) {
		log_e("Failed set nonblock\n");
		fd_gc(sock->fd);
		return -1;
	}

	memset(&sock->sin, 0, sizeof(sock->sin));
	sock->sin.sin_family = AF_INET;
	sock->sin.sin_port = htons(port);
	sock->sin.sin_addr.s_addr = htonl(INADDR_ANY);

	if (bind(sock->fd, (struct sockaddr*)&sock->sin, sizeof(sock->sin)) != 0) {
		log_e("bind error\n");
		fd_gc(sock->fd);
		return -1;
	}

	if(listen(sock->fd, 1) != 0) {
		log_e("listen error\n");
		fd_gc(sock->fd);
		return -1;
	}

	if (act) {
		sock->sel_req = sel_rd;
		sock->tdue = (tdur == aloe_dur_infinite) ? tdur : sock_tdue(tdur);
		sock->act = act;
	}

	return 0;
}

static void sock_svr_reject(int fd_svr) {
	int fd = -1;
	struct sockaddr_in sin;
	socklen_t sin_len = sizeof(sin);

	if ((fd = accept(fd_svr, (struct sockaddr*)&sin, &sin_len)) == -1) {
		log_e("Failed accept\n");
		return;
	}

	log_sockaddr("reject ", &sin);
	close(fd);
}

static int sock_svr_accept(int fd_svr, sock_t *sock,
		void (*act)(sock_t*, unsigned), unsigned long tdur) {
	socklen_t sin_len = sizeof(sock->sin);

	if ((sock->fd = accept(fd_svr, (struct sockaddr*)&sock->sin,
			&sin_len)) == -1) {
		log_e("Failed accept\n");
		return -1;
	}

	if (fd_nbio(sock->fd) != 0) {
		log_e("Failed set nonblock\n");
		fd_gc(sock->fd);
		return -1;
	}

	sock->sel_req = sel_rd;
	sock->tdue = tdur == aloe_dur_infinite ? tdur : sock_tdue(tdur);
	sock->act = act;

	return 0;
}

static int sock_cln_recv(sock_t *sock, void *data, size_t sz) {
	int r;

	r = read(sock->fd, (char*)data, sz);

	if (r == 0) {
#if 1
		log_sockaddr("cln closed ", &sock->sin);
#endif
		return -1;
	}
	if (r < 0) {
		r = errno;
		if (eno_wouldblock(r)) {
			return 0;
		}
		log_e("svc client err: %s(%d)\n",
				/*strerror(r)*/ "",
				r);
		return -1;
	}
	return r;
}

static void cln_frm_done(void *args) {
	frm_req_t *frm_req = (frm_req_t*)args;

	if (dw_spi2_req_add(&impl.frm_list, &impl.frm_lock, &frm_req->spi2_req) != 0) {
		log_e("Failed add to frm\n");
	}
}

static void cln_gc(cln_t *cln) {
	if (cln->frm) {
		cln_frm_done(cln->frm);
		cln->frm = NULL;
	}
	fd_gc(cln->sock.fd);
}

static void svc_cln_act(sock_t *_sock, unsigned actype) {
	cln_t *cln = aloe_container_of(_sock, cln_t, sock);
	aloe_buf_t *fb;
	int r = 0;
	dw_spi2_req_t *spi2_req;

	if (actype & sel_tmr) {
#if 1
		log_sockaddr("cln timeout ", &_sock->sin);
#endif
		r = 0;
		goto finally;
	}

	if (actype & sel_rd) {
		dw_pkt2_t *pkt;

		pkt = &cln->pkthdr;

		if (!cln->frm) {
			spi2_req = dw_spi2_req_pop(&impl.frm_list, &impl.frm_lock);
			if (spi2_req == NULL) {
				log_e("out of frame buffer\n");
				r = 0;
				goto finally;
			}
			cln->frm = aloe_container_of(spi2_req, frm_req_t, spi2_req);
			cln->frm->flag = 0;
			_aloe_buf_clear(&cln->frm->fb);

			// prepared to read header

			cln->pkt_lmt = 0;
		}

		if (cln->pkt_lmt < pkt2_hdr_len) {
			// read header

			if ((r = sock_cln_recv(_sock, (char*)pkt + cln->pkt_lmt,
					pkt2_hdr_len - cln->pkt_lmt)) <= 0) {
				goto finally;
			}
			cln->pkt_lmt += r;
			cln->st.acc += r;

			if (cln->pkt_lmt < pkt2_hdr_len) {
//				log_d("wait more for pkt2 hdr\n");
				r = 0;
				goto finally;
			}

			// found header, prepare to read payload (frame)

			fb = &cln->frm->fb;

			if (pkt->len > fb->cap) {
				log_e("payload length too large\n");
				r = -1;
				goto finally;
			}

			// init fb pointer
			fb->pos = 0;
			fb->lmt = pkt->len;
		} else {
			fb = &cln->frm->fb;
		}

		if (fb->lmt == 0 || fb->pos >= fb->lmt) {
			log_e("Sanity check, previous frame not process (or payload length too large)\n");
			r = -1;
			goto finally;
		}

		if ((r = sock_cln_recv(_sock, (char*)fb->data + fb->pos,
				fb->lmt - fb->pos)) <= 0) {
			goto finally;
		}
		fb->pos += r;
		cln->st.acc += r;

#if 1
		// state network speed
		{
			unsigned long ts = aloe_tick2ms(aloe_ticks());
			if (cln->st.ts_log == 0) cln->st.ts_log = ts;
			if (ts - cln->st.ts_log >= 1000) {
				// duration unit millisecond => result KBps
				double r2, rd100;

				FPS_CALC1(cln->st.acc, ts - cln->st.ts_accept);
				log_d("data rate: %d.%02dKBps\n", (int)r2, (int)rd100);

				cln->st.ts_log = aloe_tick2ms(aloe_ticks());
			}
		}
#endif
		if (fb->pos < fb->lmt) {
//			log_d("wait more for pkt2 payload\n");
			r = 0;
			goto finally;
		}

		spi2_req = &cln->frm->spi2_req;
		spi2_req->data = cln->frm->fb.data;
		spi2_req->sz = cln->frm->fb.lmt;
		spi2_req->cb = &cln_frm_done;
		spi2_req->cbarg = cln->frm;

//		log_d("frame done\n");
		cln->frm = NULL;

//		if (dw_spi2_add(spi2_req) != 0)
		{
//			log_e("Failed add to spi\n");
			cln_frm_done(spi2_req->cbarg);
			r = 0;
			goto finally;
		}
		cln->st.acc_frm_spi++;
		r = 0;
	}
	if (actype & sel_wr) {
		fb = &cln->resp;
		if (fb->lmt > fb->pos) {
			r = write(_sock->fd, (char*)fb->data + fb->pos, fb->lmt - fb->pos);
			if (r == 0) {
				r = -1;
#if 1
				log_sockaddr("cln closed ", &_sock->sin);
#endif
				goto finally;
			}
			if (r < 0) {
				r = errno;
				if (eno_wouldblock(r)) {
					r = 0;
				} else {
					log_e("svc client err: %d(0x%x)\n", r, r);
					r = -1;
				}
				goto finally;
			}
			fb->pos += r;
			aloe_buf_rewind(fb);
		}
		r = 0;
	}
finally:
	if (r < 0) {
		cln_gc(cln);
	} else {
		_sock->sel_req = sel_rd;

		// data to send
		fb = &cln->resp;
		if (fb->lmt > fb->pos) _sock->sel_req |= sel_wr;

		_sock->tdue = sock_tdue(10000);
	}
}

static void svc_accept(struct sock_rec *_sock, unsigned actype) {
	svc_t *svc = NULL;
	int i;

	if ((_sock != &impl.svc.sock)) {
		log_e("Sanity check invalid svc\n");
		return;
	}
	svc = &impl.svc;

	if (actype & sel_tmr) {
#if 0
		log_sockaddr("svc timeout ", &_sock->sin);
#endif
		goto finally;
	}

	if (actype & sel_rd) {
		cln_t *cln;

		for (i = 0; i < (int)aloe_arraysize(impl.cln); i++) {
			if (impl.cln[i].sock.fd == -1) break;
		}
		if (i >= (int)aloe_arraysize(impl.cln)) {
			log_e("all svc client slot busy\n");
			sock_svr_reject(_sock->fd);
			goto finally;
		}
		cln = &impl.cln[i];
		sinsvc_cln_reset(cln);

		if (sock_svr_accept(_sock->fd, &cln->sock
				, &svc_cln_act
				, 10000ul) != 0) {
			log_e("Failed accept svc client\n");
			goto finally;
		}

#if 1
		log_sockaddr("svc accept ", &cln->sock.sin);
#endif
		memset(&cln->st, 0, sizeof(cln->st));
		cln->st.ts_log = cln->st.ts_accept = aloe_tick2ms(aloe_ticks());
	}

finally:
	if (svc) {
		svc->sock.sel_req = sel_rd;
		svc->sock.tdue = sock_tdue(10000);
	}
}

static int svc_open(void) {
	svc_t *svc = &impl.svc;

	if (svc->sock.fd != -1) return 0;

	if (sock_svr_open(&svc->sock, DECKWIFI_SOCKET_SVC_PORT, SOCK_STREAM,
			&svc_accept, 10000ul) != 0) {
		return -1;
	}

#if 1
	log_sockaddr("svc listen on ", &svc->sock.sin);
#endif

	return 0;
}

static void mgmt_close(void) {
	mgmt_t *mgmt = &impl.mgmt;

	mgmt->sock.act = NULL;

	log_d("mgmt closed\n");
}

static void mgmt_act(struct sock_rec *_sock, unsigned actype) {
	char f_locked = 0;
	int r = 0;
	aloe_buf_t *store = &impl.mgmt.store;

	if ((_sock != &impl.mgmt.sock)) {
		log_e("Sanity check invalid mgmt\n");
		return;
	}

	if (actype & sel_tmr) {
		r = 0;
//		log_d("mgmt not busy\n");
		goto finally;
	}

	if (actype & sel_rd) {

#if 0
		// drain the event
		mgmt_event_gc(sel_rd);
#endif

		if (!f_locked) {
			if ((aloe_sem_wait(&impl.mgmt.store_lock, NULL, aloe_dur_infinite,
					"sinsvc2")) != 0) {
				log_e("lock\n");
				r = 0;
				goto finally;
			}
			f_locked = 1;
		}

		// move data to cln
		if (store->lmt > store->pos) {
			int cln_idx, rlen = store->lmt - store->pos;
			int drain_min = -1, drain_max = 0;
			void *rdata = (char*)store->data + store->pos;
			cln_t *cln;
			aloe_buf_t *fb;

			for (cln_idx = 0; cln_idx < cln_cnt; cln_idx++) {
				int wlen;

				cln = &impl.cln[cln_idx];

				if (cln->sock.fd == -1) continue;

				// any client avaliable
				fb = &cln->resp;
				wlen = fb->cap - fb->lmt;
				if (wlen > rlen) wlen = rlen;
				if (wlen > 0) {
					memcpy((char*)fb->data + fb->lmt, rdata, wlen);
					fb->lmt += wlen;

					if (drain_min < 0) {
						drain_min = drain_max = wlen;
					} else {
						if (wlen < drain_min) drain_min = wlen;
						if (wlen > drain_max) drain_max = wlen;
					}
				}
			}

			if (drain_min < 0) {
				log_e("no cln, drain all\n");
				store->pos = store->lmt;
			} else {
				if (drain_min != drain_max) {
					log_e("cln might lost data\n");
				}
				store->pos += drain_max;
			}
			aloe_buf_rewind(store);

			// still hold lock
			if (store->lmt > store->pos) {
//				mgmt_kick();
			}

			aloe_sem_post(&impl.mgmt.store_lock, NULL, "sinsvcmgmttx");
			f_locked = 0;

			// kick client do output
			for (cln_idx = 0; cln_idx < cln_cnt; cln_idx++) {
				cln = &impl.cln[cln_idx];
				if (cln->sock.fd == -1) continue;

				svc_cln_act(&cln->sock, sel_wr);
			}
		}
		r = 0;
	}
finally:
	if (f_locked) aloe_sem_post(&impl.mgmt.store_lock, NULL, "sinsvcmgmttx");
	if (r < 0) {
		mgmt_close();
	} else {
		_sock->sel_req = sel_rd;
		_sock->tdue = sock_tdue(10000);
	}
}

static int mgmt_open(void) {
	mgmt_t *mgmt = &impl.mgmt;

	if (mgmt->sock.act) return 0;

	mgmt->sock.sel_req = sel_rd;
	mgmt->sock.tdue = sock_tdue(10000ul);
	mgmt->sock.act = &mgmt_act;

	log_d("mgmt opened\n");
	return 0;
}

static int sinsvc_sock_sel(sock_t *sock, unsigned long ts0, unsigned long *tdue) {

	if (ts0 == aloe_dur_infinite) ts0 = aloe_tick2ms(aloe_ticks());

	if (sock->fd != -1) {
		sinsvc_fds(sock);
		sinsvc_tdue(*tdue, ts0, sock->tdue);
		return 1;
	}
	return 0;
}

static int sinsvc_sock_act(sock_t *sock, unsigned long ts1) {
	unsigned sel = 0;

	if (ts1 == aloe_dur_infinite) ts1 = aloe_tick2ms(aloe_ticks());

	if (sock->fd != -1) {
		sinsvc_sel(&sel, sock);
	}

	// timeout iff no sock action
	if ((sel == 0) && sinsvc_ts_tdue(ts1, sock->tdue)) sel |= sel_tmr;

	if (sel && sock->act) {
		// clear trigger
		sock->sel_req = 0;
		sock->tdue = aloe_dur_infinite;

		(*sock->act)(sock, sel);
		return 1;
	}
	return 0;
}

static void sinsvc_task(aloe_thread_t *args) {
	unsigned long ts0, ts1, tdue;
	struct timeval _tv, *tv;
	int r, i;
	char buf[50];

	(void)args;

	log_d("sinsvc2 task run, frm_req_sz: %dKB\n", frm_req_sz / 1024);

	while (!impl.quit) {
		if (!impl.launched) {
			sinsvc_close_all();
			if (sinsvc2_svcaddr(buf, sizeof(buf), NULL) != 0) {
				aloe_thread_sleep(1000);
				continue;
			}
			log_d("ipaddr ready: %s\n", buf);
		}

		if (svc_open() != 0 || mgmt_open() != 0) {
			aloe_thread_sleep(1000);
			continue;
		}
		impl.launched = 1;

		FD_ZERO(&impl.fds_rd);
		FD_ZERO(&impl.fds_wr);
		FD_ZERO(&impl.fds_ex);
		impl.fds_mx = -1;
		ts0 = aloe_tick2ms(aloe_ticks());

		// socket poll infinite bad?
		tdue = ts0 + 10000;

		// svc and client
		sinsvc_sock_sel(&impl.svc.sock, ts0, &tdue);
		for (i = 0; i < (int)aloe_arraysize(impl.cln); i++) {
			sinsvc_sock_sel(&impl.cln[i].sock, ts0, &tdue);
		}

#if 0
		// mgmt
		sinsvc_sock_sel(&impl.mgmt.sock, ts0, &tdue);
#endif

		if (impl.fds_mx == -1) {
			log_d("no socket avaliable\n");
			aloe_thread_sleep(1000);
			continue;
		}

		if (tdue == aloe_dur_infinite) {
			tv = NULL;
			log_e("Sanity check socket poll infinite\n");
#if 1
		} else if (tdue <= ts0) {
			tv = &_tv;
			tv->tv_sec = 0;
			tv->tv_usec = 0;
			log_d("socket poll immediately\n");
#endif
		} else if (tdue <= (ts0 + 100)) {
			// at least 100ms
			tv = &_tv;
			tv->tv_sec = 0;
			tv->tv_usec = 100000;
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
				aloe_thread_sleep(500);
			} else {
				log_e("select failed\n");
				impl.launched = 0;
				aloe_thread_sleep(5000);
			}
			continue;
		}
//		if (r == 0) {
//			// no trigger
//		}

		ts1 = aloe_tick2ms(aloe_ticks());

		// svc and client act
		sinsvc_sock_act(&impl.svc.sock, ts1);
		for (i = 0; i < (int)aloe_arraysize(impl.cln); i++) {
			sinsvc_sock_act(&impl.cln[i].sock, ts1);
		}

#if 0
		// mgmt act
		sinsvc_sock_act(&impl.mgmt.sock, ts1);
#endif

	}
}

ALOE_SYS_TEXT1_SECTION
int dw_sinsvc2_init(void) {
	int i;
	cln_t *cln;
	frm_req_t *frm_req;

	if (impl.ready) {
		log_e("alread initialized\n");
		return -1;
	}

	memset(&impl, 0, sizeof(impl));
	TAILQ_INIT(&impl.frm_list);

	impl.sock_list[0] = &impl.svc.sock;
	impl.sock_list[1] = &impl.mgmt.sock;
	for (i = 0; i < cln_cnt; i++) impl.sock_list[2 + i] = &impl.cln[i].sock;
	for (i = 0; i < sock_cnt; i++) impl.sock_list[i]->fd = -1;
	if (!(impl.xfer_alloc = (void*)aloe_mem_malloc(aloe_mem_id_psram,
			32 + mgmt_sz
			+ (cln_recv_sz + cln_resp_sz) * cln_cnt
			+ 32 + (sizeof(frm_req_t) + frm_req_sz) * frm_req_cnt,
			"sinsvc2"))) {
		log_e("alloc buffer\n");
		return -1;
	}

	// 32 align
	impl.xfer = (void*)aloe_roundup((unsigned long)impl.xfer_alloc, 32);

	impl.mgmt.store.data = (char*)impl.xfer;
	impl.mgmt.store.cap = mgmt_sz;

	cln = &impl.cln[0];
	cln->recv.data = (char*)impl.mgmt.store.data + impl.mgmt.store.cap;
	cln->recv.cap = cln_recv_sz;
	cln->resp.data = (char*)cln->recv.data + cln->recv.cap;
	cln->resp.cap = cln_resp_sz;
	for (i = 1; i < cln_cnt; i++) {
		cln_t *cln_prev = &impl.cln[i - 1];

		cln = &impl.cln[i];
		cln->recv.data = (char*)cln_prev->resp.data + cln_prev->resp.cap;
		cln->recv.cap = cln_recv_sz;
		cln->resp.data = (char*)cln->recv.data + cln->recv.cap;
		cln->resp.cap = cln_resp_sz;
	}

	// here cln pointer to last valid item
	frm_req = (frm_req_t*)((char*)cln->resp.data + cln->resp.cap);
	frm_req[0].fb.data = (void*)aloe_roundup((unsigned long)&frm_req[frm_req_cnt], 32);
	frm_req[0].fb.cap = frm_req_sz;
	TAILQ_INSERT_TAIL(&impl.frm_list, &frm_req[0].spi2_req, qent);
	for (i = 1; i < frm_req_cnt; i++) {
		frm_req[i].fb.data = (char*)frm_req[i - 1].fb.data + frm_req[i - 1].fb.cap;
		frm_req[i].fb.cap = frm_req_sz;
		TAILQ_INSERT_TAIL(&impl.frm_list, &frm_req[i].spi2_req, qent);
	}

	if (aloe_sem_init(&impl.frm_lock, 1, 1, "sinsvc2") != 0) {
		log_e("Failed init lock\n");
		aloe_mem_free(impl.xfer_alloc);
		return -1;
	}

	if (aloe_sem_init(&impl.mgmt.store_lock, 1, 1, "sinsvc2") != 0) {
		log_e("Failed init lock\n");
		aloe_sem_destroy(&impl.frm_lock);
		aloe_mem_free(impl.xfer_alloc);
		return -1;
	}

	if (aloe_thread_run(&impl.tsk,
			&sinsvc_task,
			4096, DECKWIFI_THREAD_PRIO_SINSVC, "sinsvc2") != 0) {
		log_e("Failed start sinsvc2 thread\n");
		aloe_sem_destroy(&impl.mgmt.store_lock);
		aloe_sem_destroy(&impl.frm_lock);
		aloe_mem_free(impl.xfer_alloc);
		return -1;
	}
	impl.ready = 1;
	return 0;
}

int dw_sinsvc2_send(const void *data, size_t size) {
	int r = -1;
	aloe_buf_t *fb = &impl.mgmt.store;

	if (impl.mgmt.sock.fd == -1) {
		log_e("mgmt not open\n");
		return -1;
	}
	if ((aloe_sem_wait(&impl.mgmt.store_lock, NULL, -1, "sinsvc2")) != 0) {
		log_e("lock\n");
		return -1;
	}
	if (fb->cap - fb->lmt < size) {
		log_e("buf full\n");
		r = -1;
		goto finally;
	}
	memcpy((char*)fb->data + fb->lmt, data, size);
	fb->lmt += size;

#if 0
	mgmt_kick();
#endif
	r = size;
finally:
	aloe_sem_post(&impl.mgmt.store_lock, NULL, "sinsvc2");
	return r;
}

int dw_sinsvc2_acc(int acc) {
	int i;
	cln_t *cln;


	for (i = 0; i < (int)aloe_arraysize(impl.cln); i++) {
		cln = &impl.cln[i];
		if (acc >= 0) cln->st.acc = acc;
		log_d("cln[%d]: %d\n", i, cln->st.acc);
	}
	return 0;
}
