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
        const cJSON *met = cJSON_GetObjectItem(root, "metric");
        const cJSON *op = cJSON_GetObjectItem(root, "op");
        const cJSON *thr = cJSON_GetObjectItem(root, "threshold");
        if (id && met && op && thr) {
            alarm_add_rule(id->valuestring, met->valuestring,
                (alarm_compare_t)op->valueint, thr->valuedouble, ALARM_SEVERITY_WARN);
            cJSON *res = cJSON_CreateObject();
            cJSON_AddStringToObject(res, "status", "added");
            char *txt = cJSON_PrintUnformatted(res);
            http_reply_json(req, 200, "OK", txt);
            free(txt); cJSON_Delete(res);
        } else {
            http_reply_json(req, 400, "Bad Request", "{&quot;error&quot;:&quot;missing fields&quot;}");
        }
    } else if (action && strcmp(action->valuestring, "query") == 0) {
        alarm_record_t recs[128];
        int n = alarm_recent(recs, 128);
        cJSON *res = cJSON_CreateObject();
        cJSON *arr = cJSON_CreateArray();
        for (int i = 0; i < n; i++) {
            cJSON *r = cJSON_CreateObject();
            cJSON_AddNumberToObject(r, "id", (double)recs[i].id);
            cJSON_AddStringToObject(r, "deviceId", recs[i].device_id);
            cJSON_AddStringToObject(r, "metric", recs[i].metric);
            cJSON_AddNumberToObject(r, "currentValue", recs[i].value);
            cJSON_AddNumberToObject(r, "threshold", recs[i].threshold);
            cJSON_AddNumberToObject(r, "severity", (int)recs[i].severity);
            cJSON_AddStringToObject(r, "message", recs[i].message);
            cJSON_AddNumberToObject(r, "triggeredAt", (double)recs[i].triggered_at);
            cJSON_AddBoolToObject(r, "acknowledged", recs[i].acknowledged);
            cJSON_AddItemToArray(arr, r);
        }
        cJSON_AddItemToObject(res, "data", arr);
        char *txt = cJSON_PrintUnformatted(res);
        http_reply_json(req, 200, "OK", txt);
        free(txt); cJSON_Delete(res);
    } else if (action && strcmp(action->valuestring, "acknowledge") == 0) {
        const cJSON *id = cJSON_GetObjectItem(root, "id");
        if (id && alarm_acknowledge((uint64_t)id->valuedouble) == 0) {
            http_reply_json(req, 200, "OK", "{&quot;status&quot;:&quot;acknowledged&quot;}");
        } else {
            http_reply_json(req, 404, "Not Found", "{&quot;error&quot;:&quot;alarm not found&quot;}");
        }
    } else {
        cJSON *res = cJSON_CreateObject();
        cJSON_AddNumberToObject(res, "total", alarm_count());
        char *txt = cJSON_PrintUnformatted(res);
        http_reply_json(req, 200, "OK", txt);
        free(txt); cJSON_Delete(res);
    }
    free(body); cJSON_Delete(root);
}
