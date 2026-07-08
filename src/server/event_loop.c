/**
 * @file event_loop.c
 *
 * Phase 1 uses kqueue (macOS) / epoll (Linux) via a tiny #ifdef split.
 * The structure is the same:
 *    1. pb_wait() returns fds ready for read/write
 *    2. listen fd ready → accept()
 *    3. conn fd ready → read → echo → flush → re-arm
 *
 * Switching to epoll later is a ~30-line swap in the PB_* calls.
 */

#define _DARWIN_C_SOURCE  /* macOS: need this before some headers */
#include "event_loop.h"
#include "connection.h"
#include "thread_pool.h"
#include "common/log.h"

#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <time.h>
#include <netinet/tcp.h>

#if defined(__linux__)
    #include <sys/epoll.h>
    #include <sys/eventfd.h>
#endif
#if defined(__APPLE__) || defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__)
    #include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
    #include <sys/event.h>
    #include <sys/time.h>
#endif

/* ------------------------------------------------------------------ */
/* per-connection state                                               */
/* ------------------------------------------------------------------ */

static int sp_init(int fds[2]) {
    if (pipe(fds) != 0) return -1;
    fcntl(fds[0], F_SETFL, O_NONBLOCK);
    fcntl(fds[1], F_SETFL, O_NONBLOCK);
    fcntl(fds[0], F_SETFD, FD_CLOEXEC);
    fcntl(fds[1], F_SETFD, FD_CLOEXEC);
    return 0;
}

static int kq_add_listen(int kq, int fd) {
#if defined(__linux__)
    struct epoll_event ev = {0};
    ev.events = EPOLLIN;
    ev.data.fd = fd;
    return epoll_ctl(kq, EPOLL_CTL_ADD, fd, &ev);
#else
    struct kevent ch;
    EV_SET(&ch, (uintptr_t)fd, EVFILT_READ, EV_ADD, 0, 0, NULL);
    return kevent(kq, &ch, 1, NULL, 0, NULL);
#endif
}


static int kq_add_conn(int kq, int fd, void *ptr, int oneshot) {
#if defined(__linux__)
    struct epoll_event ev = {0};
    ev.events = EPOLLIN | EPOLLONESHOT | EPOLLERR | EPOLLHUP;
    ev.data.ptr = ptr;
    return epoll_ctl(kq, EPOLL_CTL_ADD, fd, &ev);
#else
    (void)oneshot;
    struct kevent ch;
    EV_SET(&ch, (uintptr_t)fd, EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0, ptr);
    return kevent(kq, &ch, 1, NULL, 0, NULL);
#endif
}


static int kq_mod_in(int kq, int fd, void *ptr) {
#if defined(__linux__)
    struct epoll_event ev = {0};
    ev.events = EPOLLIN | EPOLLONESHOT | EPOLLERR | EPOLLHUP;
    ev.data.ptr = ptr;
    return epoll_ctl(kq, EPOLL_CTL_MOD, fd, &ev);
#else
    /* macOS kqueue: EVFILT_READ already armed as level-triggered; nothing to do */
    (void)kq; (void)fd; (void)ptr;
    return 0;
#endif
}


static int kq_del(int kq, int fd) {
#if defined(__linux__)
    return epoll_ctl(kq, EPOLL_CTL_DEL, fd, NULL);
#else
    struct kevent ch;
    EV_SET(&ch, (uintptr_t)fd, EVFILT_READ, EV_DELETE, 0, 0, NULL);
    return kevent(kq, &ch, 1, NULL, 0, NULL);
#endif
}

static int kq_wait(int kq, void *events, int max_events, int timeout_ms) {
#if defined(__linux__)
    struct epoll_event *ev = (struct epoll_event *)events;
    return epoll_wait(kq, ev, max_events, timeout_ms);
#else
    struct kevent *kev = (struct kevent *)events;
    struct timespec ts, *tsp = NULL;
    if (timeout_ms >= 0) {
        ts.tv_sec  = timeout_ms / 1000;
        ts.tv_nsec = (long)(timeout_ms % 1000) * 1000000L;
        tsp = &ts;
    }
    return kevent(kq, NULL, 0, kev, max_events, tsp);
#endif
}

/* ------------------------------------------------------------------ */
/* API                                                                */
/* ------------------------------------------------------------------ */
event_loop_t *event_loop_create(thread_pool_t *pool) {
    event_loop_t *loop = calloc(1, sizeof(*loop));
    if (!loop) return NULL;
    loop->pool = pool;
    loop->listen_fd = -1;
    loop->sp[0] = loop->sp[1] = -1;

#if defined(__linux__)
    loop->kq = epoll_create1(EPOLL_CLOEXEC);
#else
    loop->kq = kqueue();
#endif
    if (loop->kq < 0) { free(loop); return NULL; }

#if defined(__linux__)
    loop->events = calloc(MAX_EVENTS, sizeof(struct epoll_event));
#else
    loop->events = calloc(MAX_EVENTS, sizeof(struct kevent));
#endif
    if (!loop->events) { close(loop->kq); free(loop); return NULL; }

    if (sp_init(loop->sp) != 0) {
        LOG_ERROR("self-pipe failed: %s", strerror(errno));
    }
    return loop;
}

int event_loop_set_listen(event_loop_t *loop, int listen_fd) {
    if (!loop || listen_fd < 0) return -1;
    loop->listen_fd = listen_fd;
    if (kq_add_listen(loop->kq, listen_fd) != 0) {
        LOG_ERROR("add_listen: %s", strerror(errno));
        return -1;
    }
    if (loop->sp[0] >= 0) {
        kq_add_listen(loop->kq, loop->sp[0]);
    }
    return 0;
}

int event_loop_add_conn(event_loop_t *loop, connection_t *c) {
    if (!loop || !c) return -1;
    return kq_add_conn(loop->kq, c->fd, c, /*oneshot=*/1);
}

int event_loop_mod_in(event_loop_t *loop, connection_t *c) {
    if (!loop || !c) return -1;
    return kq_mod_in(loop->kq, c->fd, c);
}

int event_loop_del_conn(event_loop_t *loop, connection_t *c) {
    if (!loop || !c) return -1;
    kq_del(loop->kq, c->fd);
    connection_destroy(c);
    return 0;
}

static void handle_new_connection(event_loop_t *loop) {
    for (;;) {
        int cfd = accept(loop->listen_fd, NULL, NULL);
        if (cfd < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) return;
            if (errno == EINTR) continue;
            LOG_WARN("accept: %s", strerror(errno));
            return;
        }
        {
            int fl = fcntl(cfd, F_GETFL, 0);
            if (fl < 0 || fcntl(cfd, F_SETFL, fl | O_NONBLOCK) != 0) {
                LOG_ERROR("fcntl F_SETFL: %s", strerror(errno));
                close(cfd); return;
            }
            fl = fcntl(cfd, F_GETFD, 0);
            if (fl < 0 || fcntl(cfd, F_SETFD, fl | FD_CLOEXEC) != 0) {
                LOG_ERROR("fcntl F_SETFD: %s", strerror(errno));
                close(cfd); return;
            }
        }

#if !defined(MSG_NOSIGNAL)
        { int one = 1; setsockopt(cfd, SOL_SOCKET, SO_NOSIGPIPE, &one, sizeof(one)); }
#endif

        connection_t *c = connection_create(cfd);
        if (!c) { close(cfd); return; }

        /* 每个连接附带一个 MQTT wrapper;
         * 首个字节是 0x10 → 自动切到 MQTT, 否则 echo */
        conn_wrapper_t *w = conn_wrapper_create(c);
        if (!w) { connection_destroy(c); close(cfd); return; }
        c->wrapper = (void *)w;

        if (event_loop_add_conn(loop, c) != 0) {
            conn_wrapper_destroy(w);
            c->wrapper = NULL;
            connection_destroy(c);
            return;
        }
        loop->active_conns++;
        loop->total_conns++;
        LOG_INFO("conn fd=%d accepted (active=%u total=%u)",
                 cfd, loop->active_conns, loop->total_conns);
        LOG_INFO("connection object=%p fd=%d", (void *)c, c->fd);

        /* submit a pool stub so the thread_pool submission path is
         * exercised even in Phase 1 (the task js a no-op) */
        /* P1: echo handled inline; pool submission deferred */
    }
}

static void handle_disconnect(event_loop_t *loop, connection_t *c) {
    if (!c) return;
    if (c->wrapper) {
        conn_wrapper_destroy((conn_wrapper_t *)c->wrapper);
        c->wrapper = NULL;
    }
    LOG_INFO("conn fd=%d end (active=%u)", c->fd, loop->active_conns - 1);
    event_loop_del_conn(loop, c);
    loop->active_conns--;
}

static void dispatch_read(event_loop_t *loop, connection_t *c) {
    if (!c) return;

    /* 如果该连接已经有 mqtt wrapper → 走 MQTT 路径 */
    if (c->wrapper) {
        conn_wrapper_t *w = (conn_wrapper_t *)c->wrapper;
        connection_t *b = w->base;
        int r = connection_read(b);
        if (r == 0 || r < 0) {
            if (r < 0)
                LOG_ERROR("read error fd=%d errno=%d %s", c->fd, errno, strerror(errno));
            handle_disconnect(loop, c);
            return;
        }
        /* 把 [rx_off, rx_len) 这一段喂入 wrapper */
        while (b->rx_off < b->rx_len) {
            if (conn_wrapper_feed_byte(w, b->rx_buf[b->rx_off]) != 0) {
                handle_disconnect(loop, c);
                return;
            }
            b->rx_off++;
        }
        b->rx_off = b->rx_len = 0;
        event_loop_mod_in(loop, c);
        return;
    }

    /* P1 echo 路径 */
    LOG_INFO("dispatch read fd=%d", c->fd);

    int r = connection_read(c);

    if (r == 0) {
        LOG_INFO("peer closed fd=%d", c->fd);
        handle_disconnect(loop, c);
        return;
    }

    if (r < 0) {
        LOG_ERROR("read error fd=%d errno=%d %s",
                  c->fd, errno, strerror(errno));
        handle_disconnect(loop, c);
        return;
    }

    if (r > 0) {
        LOG_INFO("recv fd=%d bytes=%d", c->fd, r);

        connection_echo_run(c);

        LOG_INFO("echo prepared fd=%d tx=%zu", c->fd, c->tx_len);

        int fl = connection_flush_tx(c);

        if (fl == -1) {
            LOG_ERROR("flush failed fd=%d errno=%d %s",
                      c->fd, errno, strerror(errno));
            handle_disconnect(loop, c);
            return;
        }

        LOG_INFO("flush result fd=%d remain=%zu", c->fd, c->tx_len);
    }

    event_loop_mod_in(loop, c);
}


int event_loop_run(event_loop_t *loop) {
    if (!loop) return -1;
    loop->running = 1;
    LOG_INFO("event loop running (MAX_EVENTS=%d)...", MAX_EVENTS);

    while (loop->running) {
        int nfds = kq_wait(loop->kq, loop->events, MAX_EVENTS, 1000);
        if (nfds < 0) {
            if (errno == EINTR) continue;
            LOG_ERROR("kq_wait: %s", strerror(errno));
            return -1;
        }
        for (int i = 0; i < nfds; i++) {
            int fd; void *ptr = NULL;
#if defined(__linux__)
            struct epoll_event *ev = &((struct epoll_event *)loop->events)[i];
            fd = ev->data.fd;
            ptr = ev->data.ptr;
#else
            struct kevent *ev = &((struct kevent *)loop->events)[i];
            fd = (int)ev->ident;
            ptr = ev->udata;
#endif

            /* self-pipe wake-up → stop requested */
            if (fd == loop->sp[0]) { loop->running = 0; break; }

            /* listen fd */
            if (fd == loop->listen_fd) {
                handle_new_connection(loop);
                continue;
            }

            /* connection */
            connection_t *c = (connection_t *)ptr;
            if (!c) continue;

#if defined(__linux__)
            if (ev->events & (EPOLLERR | EPOLLHUP)) {
                handle_disconnect(loop, c);
                continue;
            }
#else
            if (ev->flags & EV_EOF) {
                handle_disconnect(loop, c);
                continue;
            }
#endif

            dispatch_read(loop, c);
        }
    }
    return 0;
}

void event_loop_stop(event_loop_t *loop) {
    if (!loop) return;
    loop->running = 0;
    if (loop->sp[1] >= 0) {
        /* write a byte to the self-pipe; signal safe */
        ssize_t s = write(loop->sp[1], "x", 1);
        (void)s;
    }
}

void event_loop_destroy(event_loop_t *loop) {
    if (!loop) return;
    if (loop->listen_fd >= 0) close(loop->listen_fd);
    if (loop->kq >= 0)  close(loop->kq);
    if (loop->sp[0] >= 0) close(loop->sp[0]);
    if (loop->sp[1] >= 0) close(loop->sp[1]);
    free(loop->events);
    free(loop);
}
