/**
 * @file handler_user.c
 *
 * 用户认证接口 — 从数据库验证用户名和密码
 */
#include "api/handlers.h"
#include "api/http_server.h"
#include "api/auth_middleware.h"
#include "data/db_pool.h"
#include "data/sql_escape.h"
#include "common/log.h"

#include <cJSON.h>
#include <stdlib.h>
#include <string.h>
#include <event2/buffer.h>
#include <mysql.h>

/**
 * 从数据库验证用户登录
 * 返回 user_id，验证失败返回 -1
 */
static int verify_user(const char *username, const char *password) {
    db_conn_t *conn = db_pool_get();
    if (!conn) {
        LOG_ERROR("DB: no connection available");
        return -1;
    }

    // 转义用户名/口令 (超长直接拒绝: 防止转义写出缓冲 + 注入)
    char esc_user[SQL_ESC_CAP(64)];
    char esc_pwd[SQL_ESC_CAP(128)];
    if (sql_escape_conn(conn, esc_user, sizeof(esc_user), username) != 0 ||
        sql_escape_conn(conn, esc_pwd, sizeof(esc_pwd), password) != 0) {
        LOG_WARN("login: username/password too long");
        db_pool_put(conn);
        return -1;
    }

    // 查询用户
    char sql[384];
    snprintf(sql, sizeof(sql),
             "SELECT id, password_hash FROM users WHERE username='%s' AND status='active' LIMIT 1",
             esc_user);

    MYSQL_RES *result = db_pool_query(conn, sql);
    if (!result) {
        LOG_ERROR("DB: query failed for user %s", username);
        db_pool_put(conn);
        return -1;
    }

    MYSQL_ROW row = mysql_fetch_row(result);
    if (!row) {
        LOG_WARN("DB: user not found: %s", username);
        db_pool_free_result(result);
        db_pool_put(conn);
        return -1;
    }

    int user_id = atoi(row[0]);
    const char *stored_hash = row[1];

    // 使用 MySQL 的 SHA2 函数计算输入密码的哈希值
    char sha_sql[512];
    snprintf(sha_sql, sizeof(sha_sql), "SELECT SHA2('%s', 256)", esc_pwd);
    MYSQL_RES *sha_result = db_pool_query(conn, sha_sql);
    if (!sha_result) {
        db_pool_free_result(result);
        db_pool_put(conn);
        return -1;
    }

    MYSQL_ROW sha_row = mysql_fetch_row(sha_result);
    if (!sha_row) {
        db_pool_free_result(sha_result);
        db_pool_free_result(result);
        db_pool_put(conn);
        return -1;
    }

    const char *input_hash = sha_row[0];
    int ok = (strcmp(input_hash, stored_hash) == 0) ? 1 : 0;

    db_pool_free_result(sha_result);
    db_pool_free_result(result);
    db_pool_put(conn);

    return ok ? user_id : -1;
}

void handler_user(struct evhttp_request *req, void *ctx) {
    (void)ctx;
    struct evbuffer *in = evhttp_request_get_input_buffer(req);
    size_t len = evbuffer_get_length(in);
    char *body = malloc(len + 1);
    evbuffer_copyout(in, body, len); body[len] = '\0';
    cJSON *root = cJSON_Parse(body);
    const cJSON *action = cJSON_GetObjectItem(root, "action");
    if (action && strcmp(action->valuestring, "login") == 0) {
        const cJSON *user = cJSON_GetObjectItem(root, "username");
        const cJSON *pass = cJSON_GetObjectItem(root, "password");
        if (!user || !pass) {
            http_reply_json(req, 400, "Bad Request", "{\"error\":\"missing username or password\"}");
            free(body); cJSON_Delete(root);
            return;
        }

        int user_id = verify_user(user->valuestring, pass->valuestring);
        if (user_id > 0) {
            char token[256];
            auth_middleware_generate_token(user->valuestring, token, sizeof(token));
            cJSON *res = cJSON_CreateObject();
            cJSON_AddStringToObject(res, "token", token);
            cJSON_AddStringToObject(res, "role", "admin");
            cJSON_AddNumberToObject(res, "user_id", user_id);
            char *txt = cJSON_PrintUnformatted(res);
            http_reply_json(req, 200, "OK", txt);
            free(txt); cJSON_Delete(res);
            LOG_INFO("User '%s' logged in (id=%d)", user->valuestring, user_id);
        } else {
            http_reply_json(req, 401, "Unauthorized", "{\"error\":\"invalid credentials\"}");
            LOG_WARN("Failed login attempt for user '%s'", user->valuestring);
        }
    } else {
        http_reply_json(req, 400, "Bad Request", "{\"error\":\"unknown action\"}");
    }
    free(body); cJSON_Delete(root);
}
