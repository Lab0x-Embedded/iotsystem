#include "api/handlers.h"
#include "api/http_server.h"
#include "business/command_service.h"
#include "server/mqtt_broker.h"
#include "common/log.h"
#include <cJSON.h>
#include <event2/buffer.h>

void handler_command(struct evhttp_request *req, void *ctx) {
    (void)ctx;
    struct evbuffer *in = evhttp_request_get_input_buffer(req);
    size_t len = evbuffer_get_length(in);
    char *body = malloc(len + 1);
    evbuffer_copyout(in, body, len); body[len] = '\0';
    cJSON *root = cJSON_Parse(body);
    const cJSON *id = cJSON_GetObjectItem(root, "device_id");
    const cJSON *cmd = cJSON_GetObjectItem(root, "cmd");
    const cJSON *payload = cJSON_GetObjectItem(root, "payload");
    if (!id || !cmd) {
        http_reply_json(req, 400, "Bad Request", "{\"error\":\"missing device_id or cmd\"}");
        free(body); cJSON_Delete(root); return;
    }
    mqtt_connection_t *conn = mqtt_broker_find_conn(id->valuestring);
    if (!conn) {
        char cmd_id[CMD_ID_LEN];
        snprintf(cmd_id, sizeof(cmd_id), "cmd_%ld", (long)time(NULL));
        cmd_mgr_enqueue_offline(id->valuestring, cmd->valuestring, payload ? payload->valuestring : "", cmd_id);
        cJSON *res = cJSON_CreateObject();
        cJSON_AddStringToObject(res, "status", "queued");
        cJSON_AddStringToObject(res, "command_id", cmd_id);
        char *txt = cJSON_PrintUnformatted(res);
        http_reply_json(req, 202, "Accepted", txt);
        free(txt); cJSON_Delete(res);
    } else {
        uint16_t pid;
        char topic[128];
        snprintf(topic, sizeof(topic), "cmd/%s/exec", id->valuestring);
        const char *pl = payload ? payload->valuestring : "{}";
        if (mqtt_broker_send_cmd(conn, topic, (const uint8_t *)pl, strlen(pl), &pid) == 0) {
            int ok = cmd_mgr_inflight_wait(conn->fd, pid, "cmd", 5);
            cJSON *res = cJSON_CreateObject();
            cJSON_AddStringToObject(res, ok ? "status" : "status", ok ? "delivered" : "timeout");
            char *txt = cJSON_PrintUnformatted(res);
            http_reply_json(req, 200, "OK", txt);
            free(txt); cJSON_Delete(res);
        } else {
            http_reply_json(req, 500, "Internal Error", "{\"error\":\"send failed\"}");
        }
    }
    free(body); cJSON_Delete(root);
}
