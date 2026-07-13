#include "api/handlers.h"
#include "api/http_server.h"
#include "data/db_pool.h"
#include "common/log.h"
#include <cJSON.h>
#include <stdlib.h>
#include <string.h>
#include <mysql.h>
#include <event2/buffer.h>

void handler_group(struct evhttp_request *req, void *ctx) {
    (void)ctx;
    struct evbuffer *in = evhttp_request_get_input_buffer(req);
    size_t len = evbuffer_get_length(in);
    char *body = malloc(len + 1);
    evbuffer_copyout(in, body, len); body[len] = '\0';
    cJSON *root = cJSON_Parse(body);
    const cJSON *action = cJSON_GetObjectItem(root, "action");
    
    if (action && strcmp(action->valuestring, "query_all") == 0) {
        /* -------- 查询所有分组 -------- */
        db_conn_t *conn = db_pool_get();
        if (!conn) {
            http_reply_json(req, 500, "Internal Error", "{\"error\":\"no db connection\"}");
            free(body); cJSON_Delete(root);
            return;
        }

        const char *sql = "SELECT g.group_id, g.group_name, g.description, "
                          "(SELECT COUNT(*) FROM devices d WHERE d.group_id = g.group_id) as device_count, "
                          "g.sort_order "
                          "FROM device_groups g WHERE g.parent_id IS NULL ORDER BY g.sort_order, g.group_id";
        
        void *result = db_pool_query(conn, sql);
        if (!result) {
            http_reply_json(req, 500, "Internal Error", "{\"error\":\"query failed\"}");
            db_pool_put(conn);
            free(body); cJSON_Delete(root);
            return;
        }

        MYSQL_RES *res = (MYSQL_RES *)result;
        MYSQL_ROW row;
        cJSON *arr = cJSON_CreateArray();

        while ((row = mysql_fetch_row(res))) {
            cJSON *item = cJSON_CreateObject();
            cJSON_AddNumberToObject(item, "group_id", atoi(row[0]));
            cJSON_AddStringToObject(item, "group_name", row[1] ? row[1] : "");
            cJSON_AddStringToObject(item, "description", row[2] ? row[2] : "");
            cJSON_AddNumberToObject(item, "device_count", row[3] ? atoi(row[3]) : 0);
            cJSON_AddNumberToObject(item, "sort_order", row[4] ? atoi(row[4]) : 0);
            cJSON_AddItemToArray(arr, item);
        }

        db_pool_free_result(result);
        db_pool_put(conn);

        cJSON *res_obj = cJSON_CreateObject();
        cJSON_AddItemToObject(res_obj, "data", arr);
        cJSON_AddNumberToObject(res_obj, "total", cJSON_GetArraySize(arr));
        char *txt = cJSON_PrintUnformatted(res_obj);
        http_reply_json(req, 200, "OK", txt);
        free(txt); cJSON_Delete(res_obj);
    }
    else if (action && strcmp(action->valuestring, "create") == 0) {
        /* -------- 创建分组（仅一级） -------- */
        const cJSON *name = cJSON_GetObjectItem(root, "name");
        const cJSON *desc = cJSON_GetObjectItem(root, "description");

        if (!name) {
            http_reply_json(req, 400, "Bad Request", "{\"error\":\"missing name\"}");
            free(body); cJSON_Delete(root);
            return;
        }

        db_conn_t *conn = db_pool_get();
        if (!conn) {
            http_reply_json(req, 500, "Internal Error", "{\"error\":\"no db connection\"}");
            free(body); cJSON_Delete(root);
            return;
        }

        char sql[512];
        snprintf(sql, sizeof(sql),
            "INSERT INTO device_groups (group_name, description) VALUES ('%s', '%s')",
            name->valuestring,
            desc ? desc->valuestring : "");

        if (db_pool_exec(conn, sql) != 0) {
            http_reply_json(req, 500, "Internal Error", "{\"error\":\"insert failed\"}");
            db_pool_put(conn);
            free(body); cJSON_Delete(root);
            return;
        }

        // 获取新插入的ID
        void *result = db_pool_query(conn, "SELECT LAST_INSERT_ID()");
        int newId = 0;
        if (result) {
            MYSQL_RES *res = (MYSQL_RES *)result;
            MYSQL_ROW row = mysql_fetch_row(res);
            if (row) newId = atoi(row[0]);
            db_pool_free_result(result);
        }
        db_pool_put(conn);

        cJSON *res_obj = cJSON_CreateObject();
        cJSON_AddNumberToObject(res_obj, "group_id", newId);
        cJSON_AddStringToObject(res_obj, "status", "created");
        char *txt = cJSON_PrintUnformatted(res_obj);
        http_reply_json(req, 200, "OK", txt);
        free(txt); cJSON_Delete(res_obj);
    }
    else if (action && strcmp(action->valuestring, "update") == 0) {
        /* -------- 更新分组 -------- */
        const cJSON *groupId = cJSON_GetObjectItem(root, "group_id");
        const cJSON *name = cJSON_GetObjectItem(root, "name");
        const cJSON *desc = cJSON_GetObjectItem(root, "description");

        if (!groupId) {
            http_reply_json(req, 400, "Bad Request", "{\"error\":\"missing group_id\"}");
            free(body); cJSON_Delete(root);
            return;
        }

        db_conn_t *conn = db_pool_get();
        if (!conn) {
            http_reply_json(req, 500, "Internal Error", "{\"error\":\"no db connection\"}");
            free(body); cJSON_Delete(root);
            return;
        }

        char sql[512];
        snprintf(sql, sizeof(sql),
            "UPDATE device_groups SET group_name='%s', description='%s' WHERE group_id=%d",
            name ? name->valuestring : "",
            desc ? desc->valuestring : "",
            groupId->valueint);

        db_pool_exec(conn, sql);
        db_pool_put(conn);

        http_reply_json(req, 200, "OK", "{\"status\":\"updated\"}");
    }
    else if (action && strcmp(action->valuestring, "delete") == 0) {
        /* -------- 删除分组 -------- */
        const cJSON *groupId = cJSON_GetObjectItem(root, "group_id");

        if (!groupId) {
            http_reply_json(req, 400, "Bad Request", "{\"error\":\"missing group_id\"}");
            free(body); cJSON_Delete(root);
            return;
        }

        db_conn_t *conn = db_pool_get();
        if (!conn) {
            http_reply_json(req, 500, "Internal Error", "{\"error\":\"no db connection\"}");
            free(body); cJSON_Delete(root);
            return;
        }

        char sql[256];
        snprintf(sql, sizeof(sql), "DELETE FROM device_groups WHERE group_id=%d", groupId->valueint);
        db_pool_exec(conn, sql);
        db_pool_put(conn);

        http_reply_json(req, 200, "OK", "{\"status\":\"deleted\"}");
    }
    else {
        http_reply_json(req, 400, "Bad Request", "{\"error\":\"unknown action\"}");
    }
    
    free(body); cJSON_Delete(root);
}
