#include "api/handlers.h"
#include "data/db_pool.h"
#include "api/http_server.h"
#include "business/device_manager.h"
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

    if (action && strcmp(action->valuestring, "register") == 0) {
        const cJSON *id = cJSON_GetObjectItem(root, "device_id");
        const cJSON *name = cJSON_GetObjectItem(root, "name");
        const cJSON *pk = cJSON_GetObjectItem(root, "product_key");
        const cJSON *dt = cJSON_GetObjectItem(root, "device_type");
        const cJSON *ds = cJSON_GetObjectItem(root, "device_secret");
        const cJSON *gid = cJSON_GetObjectItem(root, "group_id");
        if (id && pk && device_register(id->valuestring,
            name ? name->valuestring : "",
            pk->valuestring,
            dt ? dt->valuestring : "",
            ds ? ds->valuestring : "",
            gid ? gid->valueint : 0) == 0) {
            http_reply_json(req, 200, "OK", "{\"status\":\"registered\"}");
        } else {
            http_reply_json(req, 400, "Bad Request", "{\"error\":\"register failed\"}");
        }
    } else if (action && strcmp(action->valuestring, "query") == 0) {
        const cJSON *id = cJSON_GetObjectItem(root, "device_id");
        const device_info_t *d = id ? device_manager_find(id->valuestring) : NULL;
        if (d) {
            cJSON *res = cJSON_CreateObject();
            cJSON_AddStringToObject(res, "device_id", d->device_id);
            cJSON_AddStringToObject(res, "name", d->name);
            cJSON_AddStringToObject(res, "product_key", d->product_key);
            cJSON_AddStringToObject(res, "device_type", d->device_type);
            cJSON_AddStringToObject(res, "device_secret", d->device_secret);
            cJSON_AddNumberToObject(res, "group_id", d->group_id);
            cJSON_AddNumberToObject(res, "state", (int)d->state);
            cJSON_AddBoolToObject(res, "online", d->online);
            cJSON_AddNumberToObject(res, "last_active", (double)d->last_active);
            cJSON_AddNumberToObject(res, "last_online", (double)d->last_online);
            cJSON_AddNumberToObject(res, "updated_at", (double)d->updated_at);
            cJSON_AddNumberToObject(res, "report_count", d->report_count);
            char *txt = cJSON_PrintUnformatted(res);
            http_reply_json(req, 200, "OK", txt);
            free(txt); cJSON_Delete(res);
        } else {
            http_reply_json(req, 404, "Not Found", "{\"error\":\"device not found\"}");
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
        if (!id) {
            http_reply_json(req, 400, "Bad Request", "{\"error\":\"missing device_id\"}");
        } else {
            db_conn_t *conn = db_pool_get();
            if (!conn) {
                http_reply_json(req, 500, "Internal Error", "{\"error\":\"no db connection\"}");
            } else {
                char sql[512];
                if (gid) {
                    snprintf(sql, sizeof(sql),
                             "UPDATE devices SET group_id=%d WHERE device_id=\'%s\'",
                             gid->valueint, id->valuestring);
                } else if (nm) {
                    snprintf(sql, sizeof(sql),
                             "UPDATE devices SET name=\'%s\' WHERE device_id=\'%s\'",
                             nm->valuestring, id->valuestring);
                } else {
                    sql[0] = '\0';
                }
                if (sql[0] && db_pool_exec(conn, sql) == 0) {
                    http_reply_json(req, 200, "OK", "{\"status\":\"updated\"}");
                } else if (sql[0] == '\0') {
                    http_reply_json(req, 400, "Bad Request", "{\"error\":\"nothing to update\"}");
                } else {
                    http_reply_json(req, 500, "Internal Error", "{\"error\":\"update failed\"}");
                }
                db_pool_put(conn);
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
    } else {
        cJSON *res = cJSON_CreateObject();
        cJSON_AddNumberToObject(res, "online_count", device_manager_online_count());
        char *txt = cJSON_PrintUnformatted(res);
        http_reply_json(req, 200, "OK", txt);
        free(txt); cJSON_Delete(res);
    }
    free(body); cJSON_Delete(root);
}
