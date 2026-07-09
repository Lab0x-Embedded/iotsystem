#include "api/handlers.h"
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
        /* -------- 设备注册 -------- */
        const cJSON *id = cJSON_GetObjectItem(root, "device_id");
        const cJSON *name = cJSON_GetObjectItem(root, "name");
        const cJSON *pk = cJSON_GetObjectItem(root, "product_key");
        const cJSON *gid = cJSON_GetObjectItem(root, "group_id");
        if (id && pk && device_register(id->valuestring,
            name ? name->valuestring : "", pk->valuestring,
            gid ? gid->valuestring : "") == 0) {
            http_reply_json(req, 200, "OK", "{\"status\":\"registered\"}");
        } else {
            http_reply_json(req, 400, "Bad Request", "{\"error\":\"register failed\"}");
        }
    } else if (action && strcmp(action->valuestring, "query") == 0) {
        /* -------- 查询单个设备 -------- */
        const cJSON *id = cJSON_GetObjectItem(root, "device_id");
        const device_info_t *d = id ? device_manager_find(id->valuestring) : NULL;
        if (d) {
            cJSON *res = cJSON_CreateObject();
            cJSON_AddStringToObject(res, "device_id", d->device_id);
            cJSON_AddStringToObject(res, "name", d->name);
            cJSON_AddStringToObject(res, "product_key", d->product_key);
            cJSON_AddStringToObject(res, "group", d->group_id);
            cJSON_AddNumberToObject(res, "state", (int)d->state);
            cJSON_AddBoolToObject(res, "online", d->state == DEV_STATE_ONLINE);
            cJSON_AddNumberToObject(res, "report_count", d->report_count);
            char *txt = cJSON_PrintUnformatted(res);
            http_reply_json(req, 200, "OK", txt);
            free(txt); cJSON_Delete(res);
        } else {
            http_reply_json(req, 404, "Not Found", "{\"error\":\"device not found\"}");
        }
    } else if (action && strcmp(action->valuestring, "query_all") == 0) {
        /* -------- 查询所有设备 -------- */
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
            cJSON_AddStringToObject(item, "group", devices[i].group_id);
            cJSON_AddNumberToObject(item, "state", (int)devices[i].state);
            cJSON_AddBoolToObject(item, "online", devices[i].state == DEV_STATE_ONLINE);
            cJSON_AddNumberToObject(item, "report_count", devices[i].report_count);
            cJSON_AddNumberToObject(item, "last_active", (double)devices[i].last_active);
            cJSON_AddItemToArray(arr, item);
        }
        
        cJSON_AddItemToObject(res, "data", arr);
        cJSON_AddNumberToObject(res, "total", count);
        cJSON_AddNumberToObject(res, "online_count", device_manager_online_count());
        
        char *txt = cJSON_PrintUnformatted(res);
        http_reply_json(req, 200, "OK", txt);
        free(txt); cJSON_Delete(res);
    } else {
        /* -------- 默认: 返回统计信息 -------- */
        cJSON *res = cJSON_CreateObject();
        cJSON_AddNumberToObject(res, "total_count", device_manager_total_count());
        cJSON_AddNumberToObject(res, "online_count", device_manager_online_count());
        char *txt = cJSON_PrintUnformatted(res);
        http_reply_json(req, 200, "OK", txt);
        free(txt); cJSON_Delete(res);
    }
    free(body); cJSON_Delete(root);
}
