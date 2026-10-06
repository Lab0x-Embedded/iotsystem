#include "api/handlers.h"
#include "api/http_server.h"
#include "business/device_manager.h"
#include "business/alarm_service.h"
#include "business/thing_model.h"
#include "common/log.h"
#include <cJSON.h>
#include <stdlib.h>
#include <string.h>
#include <event2/buffer.h>

void handler_onenet(struct evhttp_request *req, void *ctx) {
    (void)ctx;
    struct evbuffer *in = evhttp_request_get_input_buffer(req);
    size_t len = evbuffer_get_length(in);
    char *body = malloc(len + 1);
    evbuffer_copyout(in, body, len); body[len] = '\0';
    cJSON *root = cJSON_Parse(body);
    const cJSON *id = cJSON_GetObjectItem(root, "device_id");
    const cJSON *dp = cJSON_GetObjectItem(root, "datapoints");
    if (id && dp && cJSON_IsArray(dp)) {
        /* 与 MQTT 链路同等约束：设备必须已注册，字段必须过物模型白名单 */
        device_info_t dev;
        if (device_manager_find(id->valuestring, &dev) != 0) {
            LOG_WARN("onenet sync dropped: device '%s' not registered", id->valuestring);
            http_reply_json(req, 404, "Not Found", "{\"error\":\"device not registered\"}");
            free(body); cJSON_Delete(root);
            return;
        }
        int synced = 0, rejected = 0;
        cJSON *item;
        cJSON_ArrayForEach(item, dp) {
            const cJSON *metric = cJSON_GetObjectItem(item, "metric");
            const cJSON *value = cJSON_GetObjectItem(item, "value");
            if (!metric || !value) continue;
            if (thing_model_check(dev.product_key, metric->valuestring,
                                  value->valuedouble) != TM_OK) {
                LOG_WARN("onenet sync dropped: metric '%s' not in product model",
                         metric->valuestring);
                rejected++;
                continue;
            }
            device_manager_heartbeat(id->valuestring);
            alarm_evaluate(id->valuestring, metric->valuestring, value->valuedouble);
            synced++;
        }
        char resp[96];
        snprintf(resp, sizeof(resp), "{\"status\":\"synced\",\"synced\":%d,\"rejected\":%d}",
                 synced, rejected);
        http_reply_json(req, 200, "OK", resp);
    } else {
        http_reply_json(req, 400, "Bad Request", "{\"error\":\"missing device_id or datapoints\"}");
    }
    free(body); cJSON_Delete(root);
}
