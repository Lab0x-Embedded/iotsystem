/**
 * @file handler_product.c
 *
 * 产品(products) CRUD。
 *
 * 产品与设备是一对多：devices.product_key 外键指向 products.product_key。
 *   - 新建产品后，注册设备时即可选到它
 *   - product_key 创建后不允许修改(改了两边外键就断了)
 *   - 删除产品时若下面还挂着设备则拒绝，不做级联删除
 *
 * action:
 *   query_all  产品列表 + 每个产品的设备数
 *   create     product_key* / product_name / description   (product_id 自动生成)
 *   update     按 id 改 product_name / description
 *   delete     按 id 删除(有设备引用则拒绝)
 */
#include "api/handlers.h"
#include "api/http_server.h"
#include "data/db_pool.h"
#include "data/sql_escape.h"
#include "business/thing_model.h"
#include "common/log.h"

#include <cJSON.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <mysql.h>
#include <event2/buffer.h>

/* 产品 key / id 长度上限(见 init_schema.sql: VARCHAR(64)) */
#define PROD_KEY_MAX 64

/** 生成一个不会撞车的 product_id: PROD_<8 位十六进制> (时间 + 计数打散) */
static void gen_product_id(char *out, size_t cap) {
    static unsigned long seq = 0;
    unsigned long mix = (unsigned long)time(NULL) * 2654435761UL + (++seq) * 40503UL;
    snprintf(out, cap, "PROD_%08lx", mix & 0xFFFFFFFFUL);
}

void handler_product(struct evhttp_request *req, void *ctx) {
    (void)ctx;

    struct evbuffer *in = evhttp_request_get_input_buffer(req);
    size_t len = evbuffer_get_length(in);
    char *body = malloc(len + 1);
    evbuffer_copyout(in, body, len);
    body[len] = '\0';

    cJSON *root = cJSON_Parse(body);
    const cJSON *action = cJSON_GetObjectItem(root, "action");

    db_conn_t *conn = db_pool_get();
    if (!conn) {
        http_reply_json(req, 503, "Service Unavailable",
                        "{\"error\":\"database unavailable\"}");
        free(body); cJSON_Delete(root);
        return;
    }

    /* ---------------- 查询全部产品(+设备数) ---------------- */
    if (action && strcmp(action->valuestring, "query_all") == 0) {
        const char *sql =
            "SELECT p.id, p.product_id, p.product_key, p.product_name, p.description, "
            "       (SELECT COUNT(*) FROM devices d WHERE d.product_key = p.product_key) AS device_count "
            "FROM products p ORDER BY p.id";

        MYSQL_RES *res = (MYSQL_RES *)db_pool_query(conn, sql);
        if (!res) {
            http_reply_json(req, 500, "Internal Error", "{\"error\":\"query failed\"}");
            db_pool_put(conn);
            free(body); cJSON_Delete(root);
            return;
        }

        cJSON *out = cJSON_CreateObject();
        cJSON *arr = cJSON_CreateArray();
        MYSQL_ROW row;
        int total = 0;
        while ((row = mysql_fetch_row(res)) != NULL) {
            cJSON *item = cJSON_CreateObject();
            cJSON_AddNumberToObject(item, "id", row[0] ? atoi(row[0]) : 0);
            cJSON_AddStringToObject(item, "product_id", row[1] ? row[1] : "");
            cJSON_AddStringToObject(item, "product_key", row[2] ? row[2] : "");
            cJSON_AddStringToObject(item, "product_name", row[3] ? row[3] : "");
            cJSON_AddStringToObject(item, "description", row[4] ? row[4] : "");
            cJSON_AddNumberToObject(item, "device_count", row[5] ? atoi(row[5]) : 0);
            cJSON_AddItemToArray(arr, item);
            ++total;
        }
        db_pool_free_result(res);
        db_pool_put(conn);

        cJSON_AddItemToObject(out, "data", arr);
        cJSON_AddNumberToObject(out, "total", total);
        char *txt = cJSON_PrintUnformatted(out);
        http_reply_json(req, 200, "OK", txt);
        free(txt);
        cJSON_Delete(out);
        free(body); cJSON_Delete(root);
        return;
    }

    /* ---------------- 物模型属性: 列表 / 添加 / 删除 (轻量白名单) ---------------- */
    if (action && strcmp(action->valuestring, "prop_list") == 0) {
        const cJSON *pk = cJSON_GetObjectItem(root, "product_key");
        if (!pk || !pk->valuestring || !pk->valuestring[0]) {
            http_reply_json(req, 400, "Bad Request", "{\"error\":\"missing product_key\"}");
            db_pool_put(conn);
            free(body); cJSON_Delete(root);
            return;
        }
        db_pool_put(conn);   /* thing_model 自管连接 */

        char rows[32][256];
        int n = thing_model_list(pk->valuestring, rows, 32);
        cJSON *out = cJSON_CreateObject();
        cJSON *arr = cJSON_CreateArray();
        for (int i = 0; i < n; i++) {
            char ident[64] = "", type[16] = "", desc[128] = "";
            sscanf(rows[i], "%63[^|]|%15[^|]|%127[^\n]", ident, type, desc);
            cJSON *item = cJSON_CreateObject();
            cJSON_AddStringToObject(item, "identifier", ident);
            cJSON_AddStringToObject(item, "prop_type", type);
            cJSON_AddStringToObject(item, "description", desc);
            cJSON_AddItemToArray(arr, item);
        }
        cJSON_AddStringToObject(out, "product_key", pk->valuestring);
        cJSON_AddItemToObject(out, "data", arr);
        cJSON_AddNumberToObject(out, "total", n);
        char *txt = cJSON_PrintUnformatted(out);
        http_reply_json(req, 200, "OK", txt);
        free(txt);
        cJSON_Delete(out);
        free(body); cJSON_Delete(root);
        return;
    }

    if (action && (strcmp(action->valuestring, "prop_add") == 0 ||
                   strcmp(action->valuestring, "prop_del") == 0)) {
        int is_add = (strcmp(action->valuestring, "prop_add") == 0);
        const cJSON *pk  = cJSON_GetObjectItem(root, "product_key");
        const cJSON *idn = cJSON_GetObjectItem(root, "identifier");
        const cJSON *ty  = cJSON_GetObjectItem(root, "prop_type");
        const cJSON *de  = cJSON_GetObjectItem(root, "description");

        if (!pk || !pk->valuestring[0] || !idn || !idn->valuestring[0] ||
            (is_add && (!ty || !ty->valuestring[0]))) {
            http_reply_json(req, 400, "Bad Request",
                            "{\"error\":\"missing product_key/identifier/prop_type\"}");
            db_pool_put(conn);
            free(body); cJSON_Delete(root);
            return;
        }
        db_pool_put(conn);

        int rc;
        if (is_add)
            rc = thing_model_add(pk->valuestring, idn->valuestring,
                                 ty->valuestring,
                                 de && de->valuestring ? de->valuestring : "");
        else
            rc = thing_model_delete(pk->valuestring, idn->valuestring);

        char resp[128];
        if (rc == 0 || (rc == 1 && !is_add)) {
            snprintf(resp, sizeof(resp),
                     "{\"status\":\"%s\",\"identifier\":\"%s\"}",
                     is_add ? "added" : "deleted", idn->valuestring);
            http_reply_json(req, 200, "OK", resp);
        } else {
            http_reply_json(req, 500, "Internal Error",
                            "{\"error\":\"prop operation failed (bad type? db?)\"}");
        }
        free(body); cJSON_Delete(root);
        return;
    }

    /* ---------------- 新建产品 ---------------- */
    if (action && strcmp(action->valuestring, "create") == 0) {
        const cJSON *pk   = cJSON_GetObjectItem(root, "product_key");
        const cJSON *name = cJSON_GetObjectItem(root, "product_name");
        const cJSON *desc = cJSON_GetObjectItem(root, "description");

        if (!pk || !pk->valuestring || pk->valuestring[0] == '\0') {
            http_reply_json(req, 400, "Bad Request",
                            "{\"error\":\"missing product_key\"}");
            db_pool_put(conn);
            free(body); cJSON_Delete(root);
            return;
        }

        char esc_pk[SQL_ESC_CAP(PROD_KEY_MAX)];
        char esc_name[SQL_ESC_CAP(128)];
        char esc_desc[SQL_ESC_CAP(255)];
        if (sql_escape_conn(conn, esc_pk, sizeof(esc_pk), pk->valuestring) != 0 ||
            sql_escape_conn(conn, esc_name, sizeof(esc_name),
                            name ? name->valuestring : "") != 0 ||
            sql_escape_conn(conn, esc_desc, sizeof(esc_desc),
                            desc ? desc->valuestring : "") != 0) {
            http_reply_json(req, 400, "Bad Request",
                            "{\"error\":\"parameter too long\"}");
            db_pool_put(conn);
            free(body); cJSON_Delete(root);
            return;
        }

        char pid[64];
        gen_product_id(pid, sizeof(pid));

        char sql[1024];
        snprintf(sql, sizeof(sql),
                 "INSERT INTO products (product_id, product_key, product_name, description) "
                 "VALUES ('%s', '%s', '%s', '%s')",
                 pid, esc_pk, esc_name, esc_desc);

        if (db_pool_exec(conn, sql) != 0) {
            /* 最常见原因：product_key / product_id 重复 */
            LOG_WARN("product create failed: key=%s", pk->valuestring);
            http_reply_json(req, 409, "Conflict",
                            "{\"error\":\"product_key already exists\"}");
            db_pool_put(conn);
            free(body); cJSON_Delete(root);
            return;
        }
        db_pool_put(conn);

        cJSON *out = cJSON_CreateObject();
        cJSON_AddStringToObject(out, "status", "created");
        cJSON_AddStringToObject(out, "product_id", pid);
        cJSON_AddStringToObject(out, "product_key", pk->valuestring);
        char *txt = cJSON_PrintUnformatted(out);
        http_reply_json(req, 200, "OK", txt);
        free(txt); cJSON_Delete(out);
        LOG_INFO("product created: key=%s id=%s", pk->valuestring, pid);
        free(body); cJSON_Delete(root);
        return;
    }

    /* ---------------- 修改产品(仅名称/描述) ---------------- */
    if (action && strcmp(action->valuestring, "update") == 0) {
        const cJSON *id   = cJSON_GetObjectItem(root, "id");
        const cJSON *name = cJSON_GetObjectItem(root, "product_name");
        const cJSON *desc = cJSON_GetObjectItem(root, "description");

        if (!id) {
            http_reply_json(req, 400, "Bad Request", "{\"error\":\"missing id\"}");
            db_pool_put(conn);
            free(body); cJSON_Delete(root);
            return;
        }

        char esc_name[SQL_ESC_CAP(128)];
        char esc_desc[SQL_ESC_CAP(255)];
        if (sql_escape_conn(conn, esc_name, sizeof(esc_name),
                            name ? name->valuestring : "") != 0 ||
            sql_escape_conn(conn, esc_desc, sizeof(esc_desc),
                            desc ? desc->valuestring : "") != 0) {
            http_reply_json(req, 400, "Bad Request",
                            "{\"error\":\"parameter too long\"}");
            db_pool_put(conn);
            free(body); cJSON_Delete(root);
            return;
        }

        /* product_key 故意不允许改：devices 的外键指向它 */
        char sql[640];
        snprintf(sql, sizeof(sql),
                 "UPDATE products SET product_name='%s', description='%s' WHERE id=%d",
                 esc_name, esc_desc, id->valueint);

        int rc = db_pool_exec(conn, sql);
        db_pool_put(conn);

        if (rc != 0) {
            http_reply_json(req, 500, "Internal Error", "{\"error\":\"update failed\"}");
        } else {
            http_reply_json(req, 200, "OK", "{\"status\":\"updated\"}");
        }
        free(body); cJSON_Delete(root);
        return;
    }

    /* ---------------- 删除产品(有设备则拒绝) ---------------- */
    if (action && strcmp(action->valuestring, "delete") == 0) {
        const cJSON *id = cJSON_GetObjectItem(root, "id");
        if (!id) {
            http_reply_json(req, 400, "Bad Request", "{\"error\":\"missing id\"}");
            db_pool_put(conn);
            free(body); cJSON_Delete(root);
            return;
        }

        /* 先查 product_key + 设备数 */
        char q[256];
        snprintf(q, sizeof(q),
                 "SELECT p.product_key, "
                 "       (SELECT COUNT(*) FROM devices d WHERE d.product_key = p.product_key) "
                 "FROM products p WHERE p.id=%d LIMIT 1", id->valueint);

        MYSQL_RES *res = (MYSQL_RES *)db_pool_query(conn, q);
        if (!res) {
            http_reply_json(req, 404, "Not Found", "{\"error\":\"product not found\"}");
            db_pool_put(conn);
            free(body); cJSON_Delete(root);
            return;
        }
        MYSQL_ROW row = mysql_fetch_row(res);
        int found = (row != NULL);            /* 释放结果集前取到存在性 */
        int devices = 0;
        char key[PROD_KEY_MAX] = {0};
        if (found) {
            if (row[1]) devices = atoi(row[1]);
            if (row[0]) snprintf(key, sizeof(key), "%s", row[0]);
        }
        db_pool_free_result(res);

        if (!found) {
            http_reply_json(req, 404, "Not Found", "{\"error\":\"product not found\"}");
            db_pool_put(conn);
            free(body); cJSON_Delete(root);
            return;
        }

        if (devices > 0) {
            char msg[192];
            snprintf(msg, sizeof(msg),
                     "{\"error\":\"product has devices\",\"device_count\":%d}", devices);
            http_reply_json(req, 409, "Conflict", msg);
            LOG_WARN("product delete refused: key=%s has %d device(s)", key, devices);
            db_pool_put(conn);
            free(body); cJSON_Delete(root);
            return;
        }

        char sql[256];
        snprintf(sql, sizeof(sql), "DELETE FROM products WHERE id=%d", id->valueint);
        int rc = db_pool_exec(conn, sql);
        db_pool_put(conn);

        if (rc != 0) {
            http_reply_json(req, 500, "Internal Error",
                            "{\"error\":\"delete failed (may be referenced)\"}");
        } else {
            http_reply_json(req, 200, "OK", "{\"status\":\"deleted\"}");
            LOG_INFO("product deleted: key=%s", key);
        }
        free(body); cJSON_Delete(root);
        return;
    }

    db_pool_put(conn);
    http_reply_json(req, 400, "Bad Request", "{\"error\":\"unknown action\"}");
    free(body); cJSON_Delete(root);
}
