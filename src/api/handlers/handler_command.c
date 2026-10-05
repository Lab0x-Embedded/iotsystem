/**
 * @file handler_command.c
 *
 * 指令下发 REST 入口。
 *
 * 应用层下发格式（在线直发与离线重放保持一致）：
 *   {"id":"cmd_xxx","cmd":"set_relay","payload":<原样 JSON>}
 *
 * 注意：请求里的 payload 是任意 JSON（对象/字符串/数字），
 * 不能取 valuestring —— 对象类型时它是 NULL，会导致 strlen(NULL) 崩溃。
 */
#include "api/handlers.h"
#include "api/http_server.h"
#include "business/command_service.h"
#include "server/mqtt_broker.h"
#include "common/log.h"
#include <cJSON.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <event2/buffer.h>

/* 应用层下发报文长度上限（含 cmd + payload） */
#define CMD_APP_BODY_MAX 1024

void handler_command(struct evhttp_request *req, void *ctx) {
    (void)ctx;

    struct evbuffer *in = evhttp_request_get_input_buffer(req);
    size_t len = evbuffer_get_length(in);
    char *body = malloc(len + 1);
    evbuffer_copyout(in, body, len);
    body[len] = '\0';

    cJSON *root = cJSON_Parse(body);
    const cJSON *id      = cJSON_GetObjectItem(root, "device_id");
    const cJSON *cmd     = cJSON_GetObjectItem(root, "cmd");
    const cJSON *payload = cJSON_GetObjectItem(root, "payload");

    if (!id || !cmd || !id->valuestring || !cmd->valuestring) {
        http_reply_json(req, 400, "Bad Request",
                        "{\"error\":\"missing device_id or cmd\"}");
        free(body); cJSON_Delete(root);
        return;
    }

    /* 生成指令 id */
    char cmd_id[CMD_ID_LEN];
    snprintf(cmd_id, sizeof(cmd_id), "cmd_%ld", (long)time(NULL));

    /* payload 的 JSON 文本（缺省为 null）；对象/数组/字符串/数字都原样保留 */
    char *payload_json = payload ? cJSON_PrintUnformatted(payload) : NULL;
    if (!payload_json) payload_json = strdup("null");

    /* 组装应用层下发报文 */
    char app_body[CMD_APP_BODY_MAX];
    int n = snprintf(app_body, sizeof(app_body),
                     "{\"id\":\"%s\",\"cmd\":\"%s\",\"payload\":%s}",
                     cmd_id, cmd->valuestring, payload_json);
    if (n <= 0 || (size_t)n >= sizeof(app_body)) {
        http_reply_json(req, 400, "Bad Request",
                        "{\"error\":\"command too large\"}");
        free(payload_json); free(body); cJSON_Delete(root);
        return;
    }

    /* 按 device_id 查在线连接（client_id 通常是 esp8266_<device_id>，与 device_id 不同）*/
    mqtt_connection_t *conn = mqtt_broker_find_conn_by_device(id->valuestring);

    if (!conn) {
        /* 设备离线：整条报文入队，重连订阅后重放（格式与在线直发一致） */
        cmd_mgr_enqueue_offline(id->valuestring, cmd->valuestring, payload_json, cmd_id);

        cJSON *res = cJSON_CreateObject();
        cJSON_AddStringToObject(res, "status", "queued");
        cJSON_AddStringToObject(res, "command_id", cmd_id);
        char *txt = cJSON_PrintUnformatted(res);
        http_reply_json(req, 202, "Accepted", txt);
        free(txt); cJSON_Delete(res);
    } else {
        char topic[160];
        snprintf(topic, sizeof(topic), "cmd/%s/exec", id->valuestring);

        uint16_t pid = 0;
        if (mqtt_broker_send_cmd(conn, topic,
                                 (const uint8_t *)app_body, (uint32_t)n, &pid) == 0) {
            /* QoS1：等 PUBACK（超时不代表失败，设备可能稍后回） */
            int ok = cmd_mgr_inflight_wait(conn->fd, pid, cmd_id, 5);

            cJSON *res = cJSON_CreateObject();
            cJSON_AddStringToObject(res, "status", ok ? "delivered" : "timeout");
            cJSON_AddStringToObject(res, "command_id", cmd_id);
            char *txt = cJSON_PrintUnformatted(res);
            http_reply_json(req, 200, "OK", txt);
            free(txt); cJSON_Delete(res);
        } else {
            http_reply_json(req, 500, "Internal Error", "{\"error\":\"send failed\"}");
        }
    }

    free(payload_json);
    free(body);
    cJSON_Delete(root);
}
