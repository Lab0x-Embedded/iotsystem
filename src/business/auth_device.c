/**
 * @file auth_device.c
 *
 * P4 设备认证 — 通过 DB 校验 product_key + device_secret
 *
 * 设计:
 *   - client_id = product_key/device_id
 *   - password = device_secret (明文比较, 生产环境应改为 HMAC-SHA256)
 *   - 查询 devices 表校验 product_key + device_secret
 */
#include "business/auth_device.h"
#include "data/db_pool.h"
#include "data/sql_escape.h"
#include "common/log.h"
#include <mysql.h>

#include <stdlib.h>
#include <string.h>
#include <pthread.h>

static int g_initialized = 0;

int auth_device_init(void) {
    if (g_initialized) return 0;
    g_initialized = 1;
    LOG_INFO("auth_device initialized (DB-backed)");
    return 0;
}

/**
 * 从数据库查询设备 secret.
 *  @param product_key   产品 key
 *  @param device_id     设备 id
 *  @param secret_out    输出缓冲区 (至少 256 字节)
 *  @return 0 找到, -1 未找到或错误
 */
static int lookup_secret(const char *product_key,
                         const char *device_id,
                         char *secret_out, size_t out_len) {
    if (!product_key || !device_id || !secret_out || out_len == 0) return -1;

    db_conn_t *conn = db_pool_get();
    if (!conn) {
        LOG_WARN("auth_device: no db connection");
        return -1;
    }

    char esc_pk[SQL_ESC_CAP(64)];
    char esc_id[SQL_ESC_CAP(64)];
    if (sql_escape_conn(conn, esc_pk, sizeof(esc_pk), product_key) != 0 ||
        sql_escape_conn(conn, esc_id, sizeof(esc_id), device_id) != 0) {
        LOG_WARN("auth_device: product_key/device_id too long");
        db_pool_put(conn);
        return -1;
    }

    char sql[512];
    snprintf(sql, sizeof(sql),
        "SELECT device_secret FROM devices "
        "WHERE product_key='%s' AND device_id='%s' AND status != 'decommissioned' "
        "LIMIT 1",
        esc_pk, esc_id);

    int found = -1;
    void *result = db_pool_query(conn, sql);
    if (result) {
        MYSQL_RES *res = (MYSQL_RES *)result;
        MYSQL_ROW row = mysql_fetch_row(res);
        if (row && row[0]) {
            strncpy(secret_out, row[0], out_len - 1);
            secret_out[out_len - 1] = '\0';
            found = 0;
        }
        db_pool_free_result(result);
    } else {
        LOG_WARN("auth_device: query failed for %s/%s", product_key, device_id);
    }

    db_pool_put(conn);
    return found;
}

int auth_device_verify(const char *product_key,
                       const char *device_id,
                       const char *password) {
    if (!product_key || !device_id || !password) return -1;

    char secret[256];
    if (lookup_secret(product_key, device_id, secret, sizeof(secret)) != 0) {
        LOG_WARN("auth failed: unknown device %s/%s", product_key, device_id);
        return -1;
    }

    /* 生产环境应改为 HMAC-SHA256(token, device_secret) */
    if (strcmp(password, secret) == 0) {
        LOG_INFO("auth ok: %s/%s", product_key, device_id);
        return 0;
    }

    LOG_WARN("auth failed for %s/%s: bad password", product_key, device_id);
    return -1;
}

void auth_device_shutdown(void) {
    g_initialized = 0;
    LOG_INFO("auth_device shutdown");
}
