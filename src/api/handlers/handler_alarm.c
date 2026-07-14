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
        const cJSON *sev = cJSON_GetObjectItem(root, "severity");
        if (id && met && op && thr) {
            alarm_severity_t severity = ALARM_SEVERITY_WARN;
            if (sev) {
                severity = (alarm_severity_t)sev->valueint;
            }
            alarm_add_rule(id->valuestring, met->valuestring,
                (alarm_compare_t)op->valueint, thr->valuedouble, severity);
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
            cJSON_AddNumberToObject(r, "triggeredAt", (double)recs[i].triggered_at);
            cJSON_AddBoolToObject(r, "acknowledged", recs[i].acknowledged);
            cJSON_AddStringToObject(r, "status",
                strcmp(recs[i].resolved_at, "") != 0 ? "resolved" :
                recs[i].acknowledged ? "acknowledged" : "active");
            cJSON_AddNumberToObject(r, "acknowledgedBy", recs[i].acknowledged_by);
            cJSON_AddStringToObject(r, "acknowledgedByName", recs[i].acknowledged_by_name);
            cJSON_AddStringToObject(r, "acknowledgedAt", recs[i].acknowledged_at);
            cJSON_AddNumberToObject(r, "resolvedBy", recs[i].resolved_by);
            cJSON_AddStringToObject(r, "resolvedByName", recs[i].resolved_by_name);
            cJSON_AddStringToObject(r, "resolvedAt", recs[i].resolved_at);
            cJSON_AddItemToArray(arr, r);
        }
        cJSON_AddItemToObject(res, "data", arr);
        char *txt = cJSON_PrintUnformatted(res);
        http_reply_json(req, 200, "OK", txt);
        free(txt); cJSON_Delete(res);
    } else if (action && strcmp(action->valuestring, "acknowledge") == 0) {
        const cJSON *id = cJSON_GetObjectItem(root, "id");
        const cJSON *uid = cJSON_GetObjectItem(root, "user_id");
        int user_id = uid ? uid->valueint : 0;
        if (id && alarm_acknowledge((uint64_t)id->valuedouble, user_id) == 0) {
            http_reply_json(req, 200, "OK", "{\"status\":\"acknowledged\"}");
        } else {
            http_reply_json(req, 404, "Not Found", "{\"error\":\"alarm not found\"}");
        }
    } else if (action && strcmp(action->valuestring, "resolve") == 0) {
        const cJSON *id = cJSON_GetObjectItem(root, "id");
        const cJSON *uid = cJSON_GetObjectItem(root, "user_id");
        int user_id = uid ? uid->valueint : 0;
        if (id && alarm_resolve((uint64_t)id->valuedouble, user_id) == 0) {
            http_reply_json(req, 200, "OK", "{\"status\":\"resolved\"}");
        } else {
            http_reply_json(req, 404, "Not Found", "{\"error\":\"alarm not found\"}");
        }
    } else if (action && strcmp(action->valuestring, "query_rules") == 0) {
        alarm_rule_config_t rules[64];
        int n = alarm_query_rules(rules, 64);
        cJSON *res = cJSON_CreateObject();
        cJSON *arr = cJSON_CreateArray();
        for (int i = 0; i < n; i++) {
            cJSON *r = cJSON_CreateObject();
            cJSON_AddNumberToObject(r, "id", (double)rules[i].id);
            cJSON_AddStringToObject(r, "deviceId", rules[i].device_id);
            cJSON_AddStringToObject(r, "metric", rules[i].metric);
            cJSON_AddNumberToObject(r, "op", (int)rules[i].op);
            cJSON_AddNumberToObject(r, "threshold", rules[i].threshold);
            cJSON_AddNumberToObject(r, "severity", (int)rules[i].severity);
            cJSON_AddBoolToObject(r, "enabled", rules[i].enabled);
            cJSON_AddItemToArray(arr, r);
        }
        cJSON_AddItemToObject(res, "data", arr);
        char *txt = cJSON_PrintUnformatted(res);
        http_reply_json(req, 200, "OK", txt);
        free(txt); cJSON_Delete(res);
    } else if (action && strcmp(action->valuestring, "toggle_rule") == 0) {
        const cJSON *id = cJSON_GetObjectItem(root, "id");
        if (id) {
            alarm_toggle_rule((uint64_t)id->valuedouble);
            http_reply_json(req, 200, "OK", "{\"status\":\"toggled\"}");
        } else {
            http_reply_json(req, 400, "Bad Request", "{\"error\":\"missing id\"}");
        }
    } else if (action && strcmp(action->valuestring, "edit_rule") == 0) {
        const cJSON *id = cJSON_GetObjectItem(root, "id");
        const cJSON *did = cJSON_GetObjectItem(root, "device_id");
        const cJSON *met = cJSON_GetObjectItem(root, "metric");
        const cJSON *op = cJSON_GetObjectItem(root, "op");
        const cJSON *thr = cJSON_GetObjectItem(root, "threshold");
        const cJSON *sev = cJSON_GetObjectItem(root, "severity");
        if (id && did && met && op && thr && sev) {
            alarm_edit_rule((uint64_t)id->valuedouble, did->valuestring, met->valuestring,
                (alarm_compare_t)op->valueint, thr->valuedouble,
                (alarm_severity_t)sev->valueint);
            http_reply_json(req, 200, "OK", "{\"status\":\"updated\"}");
        } else {
            http_reply_json(req, 400, "Bad Request", "{\"error\":\"missing fields\"}");
        }
    } else if (action && strcmp(action->valuestring, "delete_rule") == 0) {
        const cJSON *id = cJSON_GetObjectItem(root, "id");
        if (id && alarm_delete_rule((uint64_t)id->valuedouble) == 0) {
            http_reply_json(req, 200, "OK", "{\"status\":\"deleted\"}");
        } else {
            http_reply_json(req, 404, "Not Found", "{\"error\":\"rule not found\"}");
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
