#include "api/handlers.h"
#include "api/http_server.h"
#include "business/alarm_service.h"
#include "common/log.h"
#include <cJSON.h>
#include <stdlib.h>
#include <string.h>
#include <event2/buffer.h>

void handler_alarm(struct evhttp_request *req, void *ctx) {
    (void)ctx;
    struct evbuffer *in = evhttp_request_get_input_buffer(req);
    size_t len = evbuffer_get_length(in);
    char *body = malloc(len + 1);
    evbuffer_copyout(in, body, len); body[len] = '\0';
    cJSON *root = cJSON_Parse(body);
    const cJSON *action = cJSON_GetObjectItem(root, "action");
    if (action && strcmp(action->valuestring, "add_rule") == 0) {
        const cJSON *id = cJSON_GetObjectItem(root, "device_id");
        const cJSON *metric = cJSON_GetObjectItem(root, "metric");
        const cJSON *op = cJSON_GetObjectItem(root, "op");
        const cJSON *threshold = cJSON_GetObjectItem(root, "threshold");
        if (id && metric && op && threshold) {
            int idx = alarm_add_rule(id->valuestring, metric->valuestring,
                (alarm_compare_t)op->valueint, threshold->valuedouble, ALARM_SEVERITY_WARN);
            cJSON *res = cJSON_CreateObject();
            cJSON_AddNumberToObject(res, "rule_index", idx);
            char *txt = cJSON_PrintUnformatted(res);
            http_reply_json(req, 200, "OK", txt);
            free(txt); cJSON_Delete(res);
        } else {
            http_reply_json(req, 400, "Bad Request", "{\"error\":\"missing fields\"}");
        }
    } else {
        alarm_record_t recs[32];
        int n = alarm_recent(recs, 32);
        cJSON *res = cJSON_CreateObject();
        cJSON *arr = cJSON_CreateArray();
        for (int i = 0; i < n; i++) {
            cJSON *r = cJSON_CreateObject();
            cJSON_AddStringToObject(r, "device_id", recs[i].device_id);
            cJSON_AddStringToObject(r, "metric", recs[i].metric);
            cJSON_AddNumberToObject(r, "value", recs[i].value);
            cJSON_AddStringToObject(r, "message", recs[i].message);
            cJSON_AddItemToArray(arr, r);
        }
        cJSON_AddItemToObject(res, "alarms", arr);
        char *txt = cJSON_PrintUnformatted(res);
        http_reply_json(req, 200, "OK", txt);
        free(txt); cJSON_Delete(res);
    }
    free(body); cJSON_Delete(root);
}
