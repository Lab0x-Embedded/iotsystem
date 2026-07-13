/**
 * @file sse_handler.c
 *
 * SSE (Server-Sent Events) — 基于 libevent evhttp
 * 挂载到现有 HTTP 服务器的 /api/sse 路径
 */
#include "server/sse_handler.h"
#include "api/http_server.h"
#include "common/log.h"

#include <event2/http.h>
#include <event2/buffer.h>
#include <event2/bufferevent.h>
#include <event2/event.h>
#include <event2/keyvalq_struct.h>

#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/tcp.h>

#define SSE_MAX_CLIENTS 64

typedef struct {
    struct evhttp_request *req;
    int                    active;
} sse_client_t;

static sse_client_t  g_clients[SSE_MAX_CLIENTS];
static int           g_client_count = 0;
static pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;

/* ---- SSE 格式发送 ---- */

static void sse_send_to_client(sse_client_t *c, const char *event, const char *data) {
    if (!c || !c->active || !c->req) return;
    struct evbuffer *buf = evbuffer_new();
    if (!buf) return;
    if (event) evbuffer_add_printf(buf, "event: %s\n", event);
    evbuffer_add_printf(buf, "data: %s\n\n", data);
    evhttp_send_reply_chunk(c->req, buf);
    evbuffer_free(buf);

    /* 立即刷新到 TCP */
    struct evhttp_connection *conn = evhttp_request_get_connection(c->req);
    if (conn) {
        struct bufferevent *bev = evhttp_connection_get_bufferevent(conn);
        if (bev) {
            /* TCP_NODELAY */
            evutil_socket_t fd = bufferevent_getfd(bev);
            if (fd >= 0) {
                int flag = 1;
                setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));
            }
            /* 刷新 libevent 输出缓冲区 → 立即发送 */
            bufferevent_flush(bev, EV_WRITE, BEV_NORMAL);
        }
    }
}

static void sse_broadcast_all(const char *event, const char *data) {
    pthread_mutex_lock(&g_lock);
    for (int i = 0; i < SSE_MAX_CLIENTS; i++) {
        if (g_clients[i].active) {
            sse_send_to_client(&g_clients[i], event, data);
        }
    }
    pthread_mutex_unlock(&g_lock);
}

/* ---- SSE 请求处理 ---- */

static void sse_close_cb(struct evhttp_connection *conn, void *arg) {
    (void)conn;
    sse_client_t *c = (sse_client_t *)arg;
    if (c) {
        c->active = 0;
        c->req = NULL;
        pthread_mutex_lock(&g_lock);
        g_client_count--;
        pthread_mutex_unlock(&g_lock);
        LOG_INFO("SSE client disconnected (active=%d)", g_client_count);
    }
}

static void sse_handler(struct evhttp_request *req, void *ctx) {
    (void)ctx;

    if (evhttp_request_get_command(req) != EVHTTP_REQ_GET) {
        evhttp_send_error(req, 405, "Method Not Allowed");
        return;
    }

    struct evkeyvalq *out_hdrs = evhttp_request_get_output_headers(req);
    evhttp_add_header(out_hdrs, "Content-Type", "text/event-stream");
    evhttp_add_header(out_hdrs, "Cache-Control", "no-cache");
    evhttp_add_header(out_hdrs, "Connection", "keep-alive");
    evhttp_add_header(out_hdrs, "Access-Control-Allow-Origin", "*");

    pthread_mutex_lock(&g_lock);
    int slot = -1;
    for (int i = 0; i < SSE_MAX_CLIENTS; i++) {
        if (!g_clients[i].active) { slot = i; break; }
    }
    if (slot < 0) {
        pthread_mutex_unlock(&g_lock);
        evhttp_send_error(req, 503, "Too Many SSE Clients");
        return;
    }

    g_clients[slot].req = req;
    g_clients[slot].active = 1;
    g_client_count++;
    pthread_mutex_unlock(&g_lock);

    evhttp_send_reply_start(req, 200, "OK");

    struct evhttp_connection *conn = evhttp_request_get_connection(req);
    if (conn) {
        evhttp_connection_set_closecb(conn, sse_close_cb, &g_clients[slot]);
    }

    struct evbuffer *buf = evbuffer_new();
    evbuffer_add_printf(buf, ": connected\n\n");
    evhttp_send_reply_chunk(req, buf);
    evbuffer_free(buf);

    LOG_INFO("SSE client connected (active=%d)", g_client_count);
}

/* ---- 公开接口 ---- */

int sse_handler_init(void) {
    memset(g_clients, 0, sizeof(g_clients));
    struct evhttp *http = http_server_get_evhttp();
    if (!http) {
        LOG_ERROR("SSE: HTTP server not available");
        return -1;
    }
    evhttp_set_cb(http, "/api/sse", sse_handler, NULL);
    LOG_INFO("SSE handler registered at /api/sse");
    return 0;
}

void sse_broadcast_datapoint(const char *device_id, const char *metric,
                             double value, uint64_t ts) {
    char json[512];
    snprintf(json, sizeof(json),
        "{\"type\":\"datapoint\",\"device_id\":\"%s\",\"metric\":\"%s\","
        "\"value\":%.2f,\"ts\":%llu}",
        device_id, metric, value, (unsigned long long)ts);
    LOG_INFO("SSE broadcast: %s %s=%.2f", device_id, metric, value);
    sse_broadcast_all("datapoint", json);
}

void sse_broadcast_alarm(const char *device_id, const char *metric,
                         double value, double threshold, int severity) {
    char json[512];
    snprintf(json, sizeof(json),
        "{\"type\":\"alarm\",\"device_id\":\"%s\",\"metric\":\"%s\","
        "\"value\":%.2f,\"threshold\":%.2f,\"severity\":%d}",
        device_id, metric, value, threshold, severity);
    sse_broadcast_all("alarm", json);
}
