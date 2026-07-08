#include "api/handlers.h"
#include "api/http_server.h"
#include "business/device_manager.h"
#include "business/alarm_service.h"
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
        cJSON *item;
        cJSON_ArrayForEach(item, dp) {
            const cJSON *metric = cJSON_GetObjectItem(item, "metric");
            const cJSON *value = cJSON_GetObjectItem(item, "value");
            if (metric && value) {
                device_manager_heartbeat(id->valuestring);
                alarm_evaluate(id->valuestring, metric->valuestring, value->valuedouble);
            }
        }
        http_reply_json(req, 200, "OK", "{\"status\":\"synced\"}");
    } else {
        http_reply_json(req, 400, "Bad Request", "{\"error\":\"missing device_id or datapoints\"}");
    }
    free(body); cJSON_Delete(root);
}
