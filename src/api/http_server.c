/**
 * @file http_server.c
 *
 * HTTP REST 服务器 (libevent evhttp)
 *
 * 端点 (统一由 router_dispatch 分发):
 *   POST /api/device   (auth) — 设备注册/查询
 *   POST /api/shadow   (auth) — 设备影子 desired/reported/delta
 *   POST /api/command  (auth) — 指令下行 (在线 QoS1 / 离线入队)
 *   POST /api/alarm    (auth) — 告警规则/告警历史
 *   POST /api/user     — 登录
 *   POST /api/onenet   — OneNet 数据点同步
 *
 * 设计:
 *   - libevent evhttp 处理 HTTP 协议 (替代手写 parse/build)
 *   - 独立线程运行 event_base_dispatch
 *   - 请求统一经 router_register + router_dispatch 分发给 handler
 *   - auth_middleware 在 dispatch 层按路由表 per-route 启用
 */
#include "http_server.h"
#include "api/router.h"
#include "api/handlers.h"
#include "common/log.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>
#include <errno.h>

#include <event2/event.h>
#include <event2/http.h>
#include <event2/buffer.h>
#include <event2/keyvalq_struct.h>

/* ----------------------------------------------------------------- */
/* 全局状态                                                           */
/* ----------------------------------------------------------------- */
static struct event_base *g_base  = NULL;
static struct evhttp      *g_http = NULL;
static pthread_t           g_thread;
static volatile int        g_running = 0;

/* ----------------------------------------------------------------- */
/* HTTP 响应辅助                                                      */
/* ----------------------------------------------------------------- */
void http_reply_json(struct evhttp_request *req,
                            int code, const char *text, const char *json) {
    struct evbuffer *evbuf = evbuffer_new();
    if (!evbuf) return;
    evhttp_add_header(evhttp_request_get_output_headers(req),
                      "Content-Type", "application/json");
    evbuffer_add_printf(evbuf, "%s", json);
    evhttp_send_reply(req, code, text, evbuf);
    evbuffer_free(evbuf);
}

const char *http_method_str(enum evhttp_cmd_type t) {
    switch (t) {
        case EVHTTP_REQ_GET:     return "GET";
        case EVHTTP_REQ_POST:    return "POST";
        case EVHTTP_REQ_HEAD:    return "HEAD";
        case EVHTTP_REQ_PUT:     return "PUT";
        case EVHTTP_REQ_DELETE:  return "DELETE";
        case EVHTTP_REQ_OPTIONS: return "OPTIONS";
        case EVHTTP_REQ_PATCH:   return "PATCH";
        default:                 return "?";
    }
}

/* ----------------------------------------------------------------- */
/* OPTIONS 预检: 让浏览器/CDN 透传                                      */
/* ----------------------------------------------------------------- */
static void http_options_cb(struct evhttp_request *req, void *arg) {
    (void)arg;
    struct evkeyvalq *hdrs = evhttp_request_get_output_headers(req);
    evhttp_add_header(hdrs, "Access-Control-Allow-Origin", "*");
    evhttp_add_header(hdrs, "Access-Control-Allow-Methods", "POST, OPTIONS");
    evhttp_add_header(hdrs, "Access-Control-Allow-Headers",
                      "Authorization, Content-Type");
    struct evbuffer *evbuf = evbuffer_new();
    evhttp_send_reply(req, 204, "No Content", evbuf);
    evbuffer_free(evbuf);
}

/* ----------------------------------------------------------------- */
/* HTTP 请求分发回调 — 统一走 router                                    */
/* ----------------------------------------------------------------- */
static void http_request_cb(struct evhttp_request *req, void *arg) {
    (void)arg;
    const char *uri    = evhttp_request_get_uri(req);
    const char *method = http_method_str(evhttp_request_get_command(req));

    if (evhttp_request_get_command(req) == EVHTTP_REQ_OPTIONS) {
        http_options_cb(req, NULL);
        return;
    }

    LOG_INFO("HTTP %s %s", method, uri);

    /* 读取请求体 (供 downstream handler 解析) */
    struct evbuffer *in_buf = evhttp_request_get_input_buffer(req);
    size_t len = evbuffer_get_length(in_buf);
    if (len > 65536) {
        http_reply_json(req, 413, "Payload Too Large",
                        "{\"error\":\"body too large\"}");
        return;
    }

    /* router_dispatch 内部: resolve → auth → handler */
    router_dispatch(req);
}

/* ----------------------------------------------------------------- */
/* HTTP server 线程                                                   */
/* ----------------------------------------------------------------- */
static void *http_server_thread(void *arg) {
    (void)arg;
    g_running = 1;
    LOG_INFO("HTTP API dispatching on :%d (libevent + router)",
             g_http ? 8080 : 0);
    event_base_dispatch(g_base);
    g_running = 0;
    return NULL;
}

/* ----------------------------------------------------------------- */
/* 公开 API                                                           */
/* ----------------------------------------------------------------- */
int http_server_start(int port) {
    g_base = event_base_new();
    if (!g_base) {
        LOG_ERROR("libevent event_base_new failed");
        return -1;
    }

    g_http = evhttp_new(g_base);
    if (!g_http) {
        LOG_ERROR("libevent evhttp_new failed");
        event_base_free(g_base); g_base = NULL;
        return -1;
    }

    evhttp_set_gencb(g_http, http_request_cb, NULL);
    evhttp_set_timeout(g_http, 30);

    /* 绑定端口 */
    struct evhttp_bound_socket *handle =
        evhttp_bind_socket_with_handle(g_http, "0.0.0.0", (uint16_t)port);
    if (!handle) {
        LOG_ERROR("evhttp bind :%d failed", port);
        evhttp_free(g_http); g_http = NULL;
        event_base_free(g_base); g_base = NULL;
        return -1;
    }

    if (pthread_create(&g_thread, NULL, http_server_thread, NULL) != 0) {
        LOG_ERROR("HTTP pthread_create: %s", strerror(errno));
        evhttp_free(g_http); g_http = NULL;
        event_base_free(g_base); g_base = NULL;
        return -1;
    }
    pthread_detach(g_thread);

    LOG_INFO("HTTP API started on :%d (libevent evhttp)", port);
    return 0;
}

void http_server_stop(void) {
    g_running = 0;
    if (g_base) {
        event_base_loopbreak(g_base);
    }
}
