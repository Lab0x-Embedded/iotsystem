#include "api/handlers.h"
#include "api/http_server.h"
#include "business/shadow_manager.h"
#include "data/db_pool.h"
#include "common/log.h"
#include <cJSON.h>
#include <mysql.h>
#include <stdlib.h>
#include <string.h>
#include <event2/buffer.h>

void handler_shadow(struct evhttp_request *req, void *ctx) {
    (void)ctx;
    struct evbuffer *in = evhttp_request_get_input_buffer(req);
    size_t len = evbuffer_get_length(in);
    char *body = malloc(len + 1);
    evbuffer_copyout(in, body, len); body[len] = '\0';
    cJSON *root = cJSON_Parse(body);
    const cJSON *id = cJSON_GetObjectItem(root, "device_id");
    const cJSON *action = cJSON_GetObjectItem(root, "action");
    if (!id) { http_reply_json(req, 400, "Bad Request", "{\"error\":\"missing device_id\"}"); free(body); cJSON_Delete(root); return; }
    if (action && strcmp(action->valuestring, "set_desired") == 0) {
        const cJSON *key = cJSON_GetObjectItem(root, "key");
        const cJSON *val = cJSON_GetObjectItem(root, "value");
        if (key && val && shadow_set_desired(id->valuestring, key->valuestring, val->valuestring) == 0) {
            http_reply_json(req, 200, "OK", "{\"status\":\"desired_set\"}");
        } else {
            http_reply_json(req, 400, "Bad Request", "{\"error\":\"set_desired failed\"}");
        }
    } else if (action && strcmp(action->valuestring, "update") == 0) {
        const cJSON *desired = cJSON_GetObjectItem(root, "desired");
        if (desired && cJSON_IsObject(desired)) {
            int ok = 0;
            cJSON *child = NULL;
            cJSON_ArrayForEach(child, desired) {
                if (child->string && cJSON_IsString(child)) {
                    if (shadow_set_desired(id->valuestring, child->string, child->valuestring) == 0)
                        ok++;
                } else if (child->string) {
                    char *val = cJSON_PrintUnformatted(child);
                    if (val) {
                        shadow_set_desired(id->valuestring, child->string, val);
                        free(val);
                        ok++;
                    }
                }
            }
            cJSON *res = cJSON_CreateObject();
            cJSON_AddStringToObject(res, "status", "updated");
            cJSON_AddNumberToObject(res, "updated_keys", ok);
            char *txt = cJSON_PrintUnformatted(res);
            http_reply_json(req, 200, "OK", txt);
            free(txt); cJSON_Delete(res);
        } else {
            http_reply_json(req, 400, "Bad Request", "{\"error\":\"missing desired object\"}");
        }
    } else if (action && strcmp(action->valuestring, "delta") == 0) {
        shadow_kv_t delta[SHADOW_MAX_KVS];
        int n = shadow_compute_delta(id->valuestring, delta, SHADOW_MAX_KVS);
        cJSON *res = cJSON_CreateObject();
        cJSON *arr = cJSON_CreateArray();
        for (int i = 0; i < n; i++) {
            cJSON *kv = cJSON_CreateObject();
            cJSON_AddStringToObject(kv, "key", delta[i].key);
            cJSON_AddStringToObject(kv, "desired", delta[i].value);
            cJSON_AddItemToArray(arr, kv);
        }
        cJSON_AddItemToObject(res, "delta", arr);
        char *txt = cJSON_PrintUnformatted(res);
        http_reply_json(req, 200, "OK", txt);
        free(txt); cJSON_Delete(res);
    } else {
        /* 查询: 先从内存取 shadow, 再从 DB 取 desired/reported JSON */
        const device_shadow_t *s = shadow_find(id->valuestring);
        cJSON *res = cJSON_CreateObject();
        cJSON_AddStringToObject(res, "device_id", id->valuestring);
        if (s) cJSON_AddNumberToObject(res, "version", (double)s->version);

        /* 从 DB 读取 desired/reported */
        db_conn_t *conn = db_pool_get();
        if (conn) {
            char sql[256];
            snprintf(sql, sizeof(sql),
                "SELECT desired, reported FROM device_shadows WHERE device_id='%s' LIMIT 1",
                id->valuestring);
            void *qres = db_pool_query(conn, sql);
            if (qres) {
                MYSQL_ROW row = mysql_fetch_row((MYSQL_RES*)qres);
                if (row) {
                    const char *desired_str = row[0] ? row[0] : "{}";
                    const char *reported_str = row[1] ? row[1] : "{}";
                    cJSON *desired_json = cJSON_Parse(desired_str);
                    cJSON *reported_json = cJSON_Parse(reported_str);
                    cJSON_AddItemToObject(res, "desired", desired_json ? desired_json : cJSON_CreateObject());
                    cJSON_AddItemToObject(res, "reported", reported_json ? reported_json : cJSON_CreateObject());
                } else {
                    cJSON_AddObjectToObject(res, "desired");
                    cJSON_AddObjectToObject(res, "reported");
                }
                db_pool_free_result(qres);
            } else {
                cJSON_AddObjectToObject(res, "desired");
                cJSON_AddObjectToObject(res, "reported");
            }
            db_pool_put(conn);
        } else {
            cJSON_AddObjectToObject(res, "desired");
            cJSON_AddObjectToObject(res, "reported");
        }

        char *txt = cJSON_PrintUnformatted(res);
        http_reply_json(req, 200, "OK", txt);
        free(txt); cJSON_Delete(res);
    }
    free(body); cJSON_Delete(root);
}
