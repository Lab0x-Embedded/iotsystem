/**
 * @file http_server.c
 *
 * P5 指令下行 — HTTP REST 服务器 (libevent evhttp)
 *
 * 端点:
 *   POST /api/command
 *     Body: {"device_id":"...","cmd":"...","payload":"..."}
 *     Resp 200: {"status":"delivered","command_id":"...","acked_at":"..."}
 *     Resp 202: {"status":"queued","command_id":"...","message":"..."}
 *     Resp 400: {"error":"..."}
 *
 * 设计:
 *   - libevent evhttp 处理 HTTP 协议(替代手写 parse/build)
 *   - 独立线程运行 event_base_dispatch
 *   - 业务逻辑(设备查找/QoS 下发/等待 ack)保持不变
 */
#include "http_server.h"
#include "server/mqtt_broker.h"
#include "business/command_service.h"
#include "common/log.h"
#include <cJSON.h>

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>
#include <errno.h>
#include <time.h>

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
static uint64_t            g_cmd_seq;

/* ----------------------------------------------------------------- */
/* HTTP 响应辅助                                                      */
/* ----------------------------------------------------------------- */
static void http_reply_json(struct evhttp_request *req,
                            int code, const char *text, const char *json) {
    struct evbuffer *evbuf = evbuffer_new();
    if (!evbuf) return;
    evhttp_add_header(evhttp_request_get_output_headers(req),
                      "Content-Type", "application/json");
    evbuffer_add_printf(evbuf, "%s", json);
    evhttp_send_reply(req, code, text, evbuf);
    evbuffer_free(evbuf);
}

static const char *http_method_str(enum evhttp_cmd_type t) {
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
/* 业务逻辑: 处理一条 command 请求                                      */
/* ----------------------------------------------------------------- */
static void do_command(struct evhttp_request *req, const char *body) {
    char device_id[65] = {0}, cmd_name[64] = {0}, payload[CMD_BODY_LEN] = {0};

    /* 解析请求 JSON */
    cJSON *root = cJSON_Parse(body);
    if (!root) {
        http_reply_json(req, 400, "Bad Request", "{\"error\":\"invalid JSON\"}");
        return;
    }

    cJSON *dev_item = cJSON_GetObjectItemCaseSensitive(root, "device_id");
    cJSON *cmd_item = cJSON_GetObjectItemCaseSensitive(root, "cmd");
    if (!cJSON_IsString(dev_item) || !cJSON_IsString(cmd_item)) {
        cJSON_Delete(root);
        http_reply_json(req, 400, "Bad Request",
                        "{\"error\":\"missing device_id or cmd\"}");
        return;
    }
    strncpy(device_id, dev_item->valuestring, sizeof(device_id) - 1);
    strncpy(cmd_name, cmd_item->valuestring, sizeof(cmd_name) - 1);

    /* payload 可选, 支持 string/object/array */
    cJSON *pay_item = cJSON_GetObjectItemCaseSensitive(root, "payload");
    if (pay_item) {
        if (cJSON_IsString(pay_item)) {
            strncpy(payload, pay_item->valuestring, sizeof(payload) - 1);
        } else {
            char *s = cJSON_PrintUnformatted(pay_item);
            if (s) {
                strncpy(payload, s, sizeof(payload) - 1);
                free(s);
            }
        }
    }
    cJSON_Delete(root);

    /* 生成 command_id */
    char command_id[CMD_ID_LEN];
    snprintf(command_id, sizeof(command_id), "cmd_%lu", (unsigned long)++g_cmd_seq);

    /* 构建下发给设备的应用层 payload JSON */
    char app_payload[CMD_BODY_LEN];
    if (payload[0]) {
        snprintf(app_payload, sizeof(app_payload),
                 "{\"id\":\"%s\",\"cmd\":\"%s\",\"payload\":%s}",
                 command_id, cmd_name, payload);
    } else {
        snprintf(app_payload, sizeof(app_payload),
                 "{\"id\":\"%s\",\"cmd\":\"%s\"}",
                 command_id, cmd_name);
    }

    /* 查找在线设备 */
    mqtt_connection_t *conn = mqtt_broker_find_conn(device_id);

    if (!conn) {
        /* 离线: 入队 */
        cmd_mgr_enqueue_offline(device_id, cmd_name, payload, command_id);
        char json[512];
        snprintf(json, sizeof(json),
                 "{\"status\":\"queued\",\"command_id\":\"%s\",\"message\":\"device offline, will replay on reconnect\"}",
                 command_id);
        http_reply_json(req, 202, "Accepted", json);
        LOG_INFO("CMD queued id=%s dev=%s cmd=%s", command_id, device_id, cmd_name);
        return;
    }

    /* 在线: QoS 1 下发 + 等待 PUBACK */
    char topic[128];
    snprintf(topic, sizeof(topic), "cmd/%s/exec", device_id);

    uint16_t pid = 0;
    if (mqtt_broker_send_cmd(conn, topic,
                              (const uint8_t *)app_payload,
                              (uint32_t)strlen(app_payload),
                              &pid) != 0) {
        http_reply_json(req, 500, "Internal Error",
                        "{\"status\":\"error\",\"message\":\"send failed\"}");
        return;
    }

    int acked = cmd_mgr_inflight_wait(conn->fd, pid, command_id, 5);
    if (acked > 0) {
        char timebuf[32];
        time_t now = time(NULL);
        struct tm *tm = localtime(&now);
        strftime(timebuf, sizeof(timebuf), "%Y-%m-%dT%H:%M:%S", tm);
        char json[512];
        snprintf(json, sizeof(json),
                 "{\"status\":\"delivered\",\"command_id\":\"%s\",\"acked_at\":\"%s\"}",
                 command_id, timebuf);
        http_reply_json(req, 200, "OK", json);
        LOG_INFO("CMD delivered id=%s dev=%s cmd=%s", command_id, device_id, cmd_name);
    } else {
        char json[512];
        snprintf(json, sizeof(json),
                 "{\"status\":\"timeout\",\"command_id\":\"%s\",\"message\":\"no ack in 5s\"}",
                 command_id);
        http_reply_json(req, 200, "OK", json);
        LOG_WARN("CMD timeout id=%s dev=%s cmd=%s", command_id, device_id, cmd_name);
    }
}

/* ----------------------------------------------------------------- */
/* HTTP 请求分发回调                                                   */
/* ----------------------------------------------------------------- */
static void http_request_cb(struct evhttp_request *req, void *arg) {
    (void)arg;
    const char *uri = evhttp_request_get_uri(req);
    enum evhttp_cmd_type method = evhttp_request_get_command(req);

    LOG_INFO("HTTP %s %s", http_method_str(method), uri);

    if (method != EVHTTP_REQ_POST || strcmp(uri, "/api/command") != 0) {
        evhttp_send_error(req, 404, "Not Found");
        return;
    }

    /* 读取请求体 */
    struct evbuffer *in_buf = evhttp_request_get_input_buffer(req);
    size_t len = evbuffer_get_length(in_buf);
    if (len == 0 || len > 65536) {
        evhttp_send_error(req, 400, "Bad Request");
        return;
    }

    char *body = malloc(len + 1);
    if (!body) {
        evhttp_send_error(req, 500, "Internal Error");
        return;
    }
    evbuffer_copyout(in_buf, body, len);
    body[len] = '\0';

    do_command(req, body);
    free(body);
}

/* ----------------------------------------------------------------- */
/* HTTP server 线程                                                   */
/* ----------------------------------------------------------------- */
static void *http_server_thread(void *arg) {
    (void)arg;
    g_running = 1;
    LOG_INFO("HTTP API dispatching on :8080 (libevent)");
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
