#include "api/handlers.h"
#include "data/query_service.h"
#include "data/db_pool.h"
#include <mysql.h>
#include "api/http_server.h"
#include "business/device_manager.h"
#include "mqtt/mqtt_types.h"
#include "common/log.h"
#include <cJSON.h>
#include <stdlib.h>
#include <string.h>
#include <event2/buffer.h>

void handler_device(struct evhttp_request *req, void *ctx) {
    (void)ctx;
    struct evbuffer *in = evhttp_request_get_input_buffer(req);
    size_t len = evbuffer_get_length(in);
    char *body = malloc(len + 1);
    evbuffer_copyout(in, body, len); body[len] = '\0';
    cJSON *root = cJSON_Parse(body);
    const cJSON *action = cJSON_GetObjectItem(root, "action");

    if (action && strcmp(action->valuestring, "activate") == 0) {
        /* -------- 设备激活（上线) - 校验 device_secret -------- */
        const cJSON *id = cJSON_GetObjectItem(root, "device_id");
        const cJSON *secret = cJSON_GetObjectItem(root, "device_secret");
        if (!id) {
            http_reply_json(req, 400, "Bad Request", "{\"error\":\"missing device_id\"}");
            free(body); cJSON_Delete(root);
            return;
        }

        /* 激活逻辑收敛到 device_manager (含 secret 校验 + 转义) */
        int rc = device_manager_activate(id->valuestring,
                                         secret ? secret->valuestring : NULL);
        if (rc == 0) {
            http_reply_json(req, 200, "OK", "{\"status\":\"activated\"}");
            LOG_INFO("Device %s activated (secret=%s)", id->valuestring,
                     secret ? "checked" : "skipped");
        } else if (rc == -1) {
            http_reply_json(req, 404, "Not Found",
                            "{\"error\":\"device not found or secret mismatch\"}");
            LOG_WARN("Activation failed for device %s: not found or secret mismatch",
                     id->valuestring);
        } else {
            http_reply_json(req, 500, "Internal Error", "{\"error\":\"activate failed\"}");
        }
    } else if (action && strcmp(action->valuestring, "register") == 0) {
        const cJSON *id = cJSON_GetObjectItem(root, "device_id");
        const cJSON *name = cJSON_GetObjectItem(root, "name");
        const cJSON *pk = cJSON_GetObjectItem(root, "product_key");
        const cJSON *dt = cJSON_GetObjectItem(root, "device_type");
        const cJSON *ds = cJSON_GetObjectItem(root, "device_secret");
        const cJSON *gid = cJSON_GetObjectItem(root, "group_id");
        if (id && pk) {
            /* 未提供密钥时服务端自动生成；返回实际生效的密钥，
             * 否则调用方拿不到密钥、无法配置设备接入。 */
            char secret[MQTT_ID_MAX] = {0};
            if (device_register(id->valuestring,
                                name ? name->valuestring : "",
                                pk->valuestring,
                                dt ? dt->valuestring : "",
                                ds ? ds->valuestring : "",
                                gid ? gid->valueint : 0,
                                secret, sizeof(secret)) == 0) {
                cJSON *res = cJSON_CreateObject();
                cJSON_AddStringToObject(res, "status", "registered");
                cJSON_AddStringToObject(res, "device_id", id->valuestring);
                cJSON_AddStringToObject(res, "product_key", pk->valuestring);
                cJSON_AddStringToObject(res, "device_secret", secret);
                char *txt = cJSON_PrintUnformatted(res);
                http_reply_json(req, 200, "OK", txt);
                free(txt); cJSON_Delete(res);
            } else {
                http_reply_json(req, 400, "Bad Request",
                                "{\"error\":\"register failed\"}");
            }
        } else {
            http_reply_json(req, 400, "Bad Request",
                            "{\"error\":\"missing device_id or product_key\"}");
        }
    } else if (action && strcmp(action->valuestring, "query") == 0) {
        const cJSON *id = cJSON_GetObjectItem(root, "device_id");
        device_info_t d;
        if (id && device_manager_find(id->valuestring, &d) == 0) {
            cJSON *res = cJSON_CreateObject();
            cJSON_AddStringToObject(res, "device_id", d.device_id);
            cJSON_AddStringToObject(res, "name", d.name);
            cJSON_AddStringToObject(res, "product_key", d.product_key);
            cJSON_AddStringToObject(res, "device_type", d.device_type);
            cJSON_AddStringToObject(res, "device_secret", d.device_secret);
            cJSON_AddNumberToObject(res, "group_id", d.group_id);
            cJSON_AddNumberToObject(res, "state", (int)d.state);
            cJSON_AddBoolToObject(res, "online", d.online);
            cJSON_AddNumberToObject(res, "last_active", (double)d.last_active);
            cJSON_AddNumberToObject(res, "last_online", (double)d.last_online);
            cJSON_AddNumberToObject(res, "updated_at", (double)d.updated_at);
            cJSON_AddNumberToObject(res, "report_count", d.report_count);
            char *txt = cJSON_PrintUnformatted(res);
            http_reply_json(req, 200, "OK", txt);
            free(txt); cJSON_Delete(res);
        } else {
            http_reply_json(req, 404, "Not Found", "{\"error\":\"device not found\"}");
        }
    } else if (action && strcmp(action->valuestring, "query_latest") == 0) {
        /* -------- 轮询数据源: 全量设备最新数据点 (device_latest_data) -------- */
        db_conn_t *db = db_pool_get();
        if (!db) {
            http_reply_json(req, 503, "Service Unavailable",
                            "{\"error\":\"no db connection\"}");
        } else {
            MYSQL_RES *res = (MYSQL_RES *)db_pool_query(db,
                "SELECT device_id, metric, value, ts FROM device_latest_data");
            cJSON *out = cJSON_CreateObject();
            cJSON *arr = cJSON_CreateArray();
            int total = 0;
            if (res) {
                MYSQL_ROW row;
                while ((row = mysql_fetch_row(res)) != NULL) {
                    cJSON *item = cJSON_CreateObject();
                    cJSON_AddStringToObject(item, "device_id", row[0] ? row[0] : "");
                    cJSON_AddStringToObject(item, "metric", row[1] ? row[1] : "");
                    cJSON_AddNumberToObject(item, "value", row[2] ? atof(row[2]) : 0);
                    cJSON_AddNumberToObject(item, "ts", row[3] ? (double)strtoull(row[3], NULL, 10) : 0);
                    cJSON_AddItemToArray(arr, item);
                    total++;
                }
                db_pool_free_result(res);
            }
            db_pool_put(db);
            cJSON_AddItemToObject(out, "data", arr);
            cJSON_AddNumberToObject(out, "total", total);
            char *txt = cJSON_PrintUnformatted(out);
            http_reply_json(req, 200, "OK", txt);
            free(txt);
            cJSON_Delete(out);
        }
    } else if (action && strcmp(action->valuestring, "query_all") == 0) {
        const device_info_t *devices = NULL;
        int count = 0;
        device_manager_get_all(&devices, &count);
        cJSON *res = cJSON_CreateObject();
        cJSON *arr = cJSON_CreateArray();
        for (int i = 0; i < count; i++) {
            cJSON *item = cJSON_CreateObject();
            cJSON_AddStringToObject(item, "device_id", devices[i].device_id);
            cJSON_AddStringToObject(item, "name", devices[i].name);
            cJSON_AddStringToObject(item, "product_key", devices[i].product_key);
            cJSON_AddStringToObject(item, "device_type", devices[i].device_type);
            cJSON_AddStringToObject(item, "device_secret", devices[i].device_secret);
            cJSON_AddNumberToObject(item, "group_id", devices[i].group_id);
            cJSON_AddNumberToObject(item, "state", (int)devices[i].state);
            cJSON_AddBoolToObject(item, "online", devices[i].online);
            cJSON_AddNumberToObject(item, "last_active", (double)devices[i].last_active);
            cJSON_AddNumberToObject(item, "last_online", (double)devices[i].last_online);
            cJSON_AddNumberToObject(item, "report_count", devices[i].report_count);
            cJSON_AddItemToArray(arr, item);
        }
        cJSON_AddItemToObject(res, "data", arr);
        cJSON_AddNumberToObject(res, "total", count);
        cJSON_AddNumberToObject(res, "online_count", device_manager_online_count());
        char *txt = cJSON_PrintUnformatted(res);
        http_reply_json(req, 200, "OK", txt);
        free(txt); cJSON_Delete(res);
        free((void*)devices);
    } else if (action && strcmp(action->valuestring, "decommission") == 0) {
        const cJSON *id = cJSON_GetObjectItem(root, "device_id");
        if (id && device_manager_decommission(id->valuestring) == 0) {
            http_reply_json(req, 200, "OK", "{\"status\":\"decommissioned\"}");
        } else {
            http_reply_json(req, 400, "Bad Request", "{\"error\":\"decommission failed\"}");
        }
    } else if (action && strcmp(action->valuestring, "update") == 0) {
        const cJSON *id  = cJSON_GetObjectItem(root, "device_id");
        const cJSON *gid = cJSON_GetObjectItem(root, "group_id");
        const cJSON *nm  = cJSON_GetObjectItem(root, "name");
        if (!id || !id->valuestring) {
            http_reply_json(req, 400, "Bad Request", "{\"error\":\"missing device_id\"}");
        } else {
            /* 收敛到 device_manager: 修掉 `SET name=` 列名错误 + 统一转义 */
            int rc = -1;
            if (gid) {
                rc = device_manager_update_group(id->valuestring, gid->valueint);
            } else if (nm && nm->valuestring) {
                rc = device_manager_update_name(id->valuestring, nm->valuestring);
            }
            if (rc == 0) {
                http_reply_json(req, 200, "OK", "{\"status\":\"updated\"}");
            } else if (!gid && (!nm || !nm->valuestring)) {
                http_reply_json(req, 400, "Bad Request", "{\"error\":\"nothing to update\"}");
            } else {
                http_reply_json(req, 500, "Internal Error", "{\"error\":\"update failed\"}");
            }
        }
    } else if (action && strcmp(action->valuestring, "query_by_group") == 0) {
        const cJSON *gid = cJSON_GetObjectItem(root, "group_id");
        if (!gid) {
            http_reply_json(req, 400, "Bad Request", "{\"error\":\"missing group_id\"}");
        } else {
            const device_info_t *devices = NULL;
            int count = 0;
            device_manager_get_by_group(gid->valueint, &devices, &count);
            cJSON *res = cJSON_CreateObject();
            cJSON *arr = cJSON_CreateArray();
            for (int i = 0; i < count; i++) {
                cJSON *item = cJSON_CreateObject();
                cJSON_AddStringToObject(item, "device_id", devices[i].device_id);
                cJSON_AddStringToObject(item, "name", devices[i].name);
                cJSON_AddStringToObject(item, "product_key", devices[i].product_key);
                cJSON_AddStringToObject(item, "device_type", devices[i].device_type);
                cJSON_AddStringToObject(item, "device_secret", devices[i].device_secret);
                cJSON_AddNumberToObject(item, "group_id", devices[i].group_id);
                cJSON_AddNumberToObject(item, "state", (int)devices[i].state);
                cJSON_AddBoolToObject(item, "online", devices[i].online);
                cJSON_AddNumberToObject(item, "last_active", (double)devices[i].last_active);
                cJSON_AddNumberToObject(item, "last_online", (double)devices[i].last_online);
                cJSON_AddNumberToObject(item, "report_count", devices[i].report_count);
                cJSON_AddItemToArray(arr, item);
            }
            cJSON_AddItemToObject(res, "data", arr);
            cJSON_AddNumberToObject(res, "total", count);
            cJSON_AddNumberToObject(res, "group_id", gid->valueint);
            char *txt = cJSON_PrintUnformatted(res);
            http_reply_json(req, 200, "OK", txt);
            free(txt); cJSON_Delete(res);
            free((void*)devices);
        }
    } else if (action && strcmp(action->valuestring, "query_history") == 0) {
        const cJSON *did = cJSON_GetObjectItem(root, "device_id");
        const cJSON *met = cJSON_GetObjectItem(root, "metric");
        const cJSON *start = cJSON_GetObjectItem(root, "start_ts");
        const cJSON *end = cJSON_GetObjectItem(root, "end_ts");
        const cJSON *lim = cJSON_GetObjectItem(root, "limit");
        if (did && met && start && end) {
            query_request_t req2;
            memset(&req2, 0, sizeof(req2));
            strncpy(req2.device_id, did->valuestring, 64);
            strncpy(req2.metric, met->valuestring, 63);
            req2.start_ts = (uint64_t)start->valuedouble;
            req2.end_ts = (uint64_t)end->valuedouble;
            req2.limit = lim ? lim->valueint : 200;
            query_point_t points[QUERY_POINTS_MAX];
            int n = query_history(&req2, points, QUERY_POINTS_MAX);
            cJSON *res = cJSON_CreateObject();
            cJSON *arr = cJSON_CreateArray();
            for (int i = 0; i < n; i++) {
                cJSON *p = cJSON_CreateObject();
                cJSON_AddNumberToObject(p, "ts", (double)points[i].ts);
                cJSON_AddNumberToObject(p, "value", points[i].value);
                cJSON_AddItemToArray(arr, p);
            }
            cJSON_AddItemToObject(res, "data", arr);
            cJSON_AddNumberToObject(res, "total", n);
            char *txt = cJSON_PrintUnformatted(res);
            http_reply_json(req, 200, "OK", txt);
            free(txt); cJSON_Delete(res);
        } else {
            http_reply_json(req, 400, "Bad Request", "{\"error\":\"missing fields\"}");
        }
    } else {
        cJSON *res = cJSON_CreateObject();
        cJSON_AddNumberToObject(res, "online_count", device_manager_online_count());
        char *txt = cJSON_PrintUnformatted(res);
        http_reply_json(req, 200, "OK", txt);
        free(txt); cJSON_Delete(res);
    }
    free(body); cJSON_Delete(root);
}
