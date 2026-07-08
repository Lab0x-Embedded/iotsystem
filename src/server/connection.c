/**
 * @file connection.c - minimal connection handle
 */
#include "connection.h"
#include "common/log.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include <sys/socket.h>

#define RX_INIT (4*1024)
#define RX_MAX  (64*1024)

/* cross-platform: ignore SIGPIPE on send()
 *   Linux  : MSG_NOSIGNAL
 *   macOS  : SO_NOSIGPIPE (set once per socket)
 */
static ssize_t safe_send(int fd, const void *buf, size_t len) {
#if defined(MSG_NOSIGNAL)
    return send(fd, buf, len, MSG_NOSIGNAL);
#else
    return send(fd, buf, len, 0);
#endif
}

connection_t *connection_create(int fd) {
    connection_t *c = calloc(1, sizeof(*c));
    if (!c) { LOG_ERROR("calloc connection: %s", strerror(errno)); return NULL; }
    c->fd = fd;
    c->last_active = time(NULL);
    c->rx_buf = malloc(RX_INIT);
    if (!c->rx_buf) { LOG_ERROR("malloc rx_buf OOM"); free(c); return NULL; }
    c->tx_buf = malloc(RX_INIT);
    if (!c->tx_buf) { LOG_ERROR("malloc tx_buf OOM"); free(c->rx_buf); free(c); return NULL; }
    c->rx_cap = RX_INIT;
    c->tx_cap = RX_INIT;
    return c;
}

void connection_destroy(connection_t *c) {
    if (!c) return;
    if (c->fd >= 0) close(c->fd);
    free(c->rx_buf); free(c->tx_buf);
    free(c);
}

/* drain kernel RX into c->rx_buf; returns:>0 got bytes, 0 peer gone, -1 err */
int connection_read(connection_t *c) {
    char tmp[4096];
    int total = 0;
    for (;;) {
        ssize_t n = recv(c->fd, tmp, sizeof(tmp), 0);
        if (n > 0) {
            if (c->rx_len + (size_t)n > c->rx_cap) {
                size_t nc = c->rx_cap * 2;
                /* 如果 offset 前面有垃圾数据就滑动一下 */
                if (c->rx_off > 0) {
                    memmove(c->rx_buf, c->rx_buf + c->rx_off, c->rx_len - c->rx_off);
                    c->rx_len -= c->rx_off;
                    c->rx_off = 0;
                }
                if (c->rx_len + (size_t)n > c->rx_cap) {
                    nc = c->rx_cap * 2;
                    if (nc > RX_MAX) return -1;
                    unsigned char *nb = realloc(c->rx_buf, nc);
                    if (!nb) return -1;
                    c->rx_buf = nb; c->rx_cap = nc;
                }
            }
            memcpy(c->rx_buf + c->rx_len, tmp, n);
            c->rx_len += (size_t)n;
            total += (int)n;
            continue;
        }
        if (n == 0) return 0;
        if (errno == EAGAIN || errno == EWOULDBLOCK) return total;
        if (errno == EINTR) continue;
        return -1;
    }
}

void connection_echo_run(connection_t *c) {
    if (c->rx_off >= c->rx_len) { c->rx_off = c->rx_len = 0; return; }
    size_t data_len = c->rx_len - c->rx_off;
    if (c->tx_len + data_len > (size_t)c->tx_cap) {
        size_t nc = c->tx_cap ? c->tx_cap : 4096;
        while (nc < c->tx_len + data_len) nc *= 2;
        unsigned char *nb = realloc(c->tx_buf, nc);
        if (!nb) return;
        c->tx_buf = nb; c->tx_cap = (int)nc;
    }
    memcpy(c->tx_buf + c->tx_len, c->rx_buf + c->rx_off, data_len);
    c->tx_len += data_len;
    c->rx_off = c->rx_len;
    c->last_active = time(NULL);
}

/* try flush pending tx; returns: 1 drained, 0 partial, -1 err */
int connection_flush_tx(connection_t *c) {
    if (c->tx_len == 0) return 1;
    ssize_t n = safe_send(c->fd, c->tx_buf, c->tx_len);
    if (n > 0) {
        if ((size_t)n < c->tx_len)
            memmove(c->tx_buf, c->tx_buf + n, c->tx_len - (size_t)n);
        c->tx_len -= (size_t)n;
        return c->tx_len == 0 ? 1 : 0;
    }
    if (errno == EAGAIN || errno == EWOULDBLOCK) return 0;
    if (errno == EINTR) return 0;

    LOG_ERROR("send failed fd=%d errno=%d %s",
              c->fd, errno, strerror(errno));

    return -1;
}
