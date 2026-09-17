/**
 * @file shadow_manager.c
 *
 * P4 设备影子 — 内存 + DB 持久化
 */
#include "business/shadow_manager.h"
#include "data/db_pool.h"
#include "data/sql_escape.h"
#include "common/log.h"

#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <cJSON.h>
#include <mysql.h>

#define MAX_SHADOWS 512

static device_shadow_t g_shadows[MAX_SHADOWS];
static int              g_shadow_count = 0;
static pthread_mutex_t  g_lock = PTHREAD_MUTEX_INITIALIZER;

int shadow_manager_init(void) {
    pthread_mutex_lock(&g_lock);
    memset(g_shadows, 0, sizeof(g_shadows));
    g_shadow_count = 0;
    pthread_mutex_unlock(&g_lock);
    LOG_INFO("shadow_manager initialized (capacity=%d)", MAX_SHADOWS);
    return 0;
}

static device_shadow_t *shadow_get_or_create(const char *device_id) {
    for (int i = 0; i < g_shadow_count; i++)
        if (strcmp(g_shadows[i].device_id, device_id) == 0) return &g_shadows[i];
    if (g_shadow_count >= MAX_SHADOWS) return NULL;
    device_shadow_t *s = &g_shadows[g_shadow_count++];
    memset(s, 0, sizeof(*s));
    strncpy(s->device_id, device_id, SHADOW_DEV_LEN - 1);
    return s;
}

static shadow_kv_t *kv_find(shadow_kv_t *arr, int n, const char *key) {
    for (int i = 0; i < n; i++)
        if (strcmp(arr[i].key, key) == 0) return &arr[i];
    return NULL;
}

int shadow_set_desired(const char *device_id, const char *key, const char *value) {
    if (!device_id || !key || !value) return -1;
    pthread_mutex_lock(&g_lock);
    device_shadow_t *s = shadow_get_or_create(device_id);
    if (!s) { pthread_mutex_unlock(&g_lock); return -1; }
    shadow_kv_t *kv = kv_find(s->desired, s->desired_n, key);
    if (kv) {
        strncpy(kv->value, value, SHADOW_VAL_LEN - 1);
        kv->version++;
    } else if (s->desired_n < SHADOW_MAX_KVS) {
        kv = &s->desired[s->desired_n++];
        strncpy(kv->key, key, SHADOW_KEY_LEN - 1);
        strncpy(kv->value, value, SHADOW_VAL_LEN - 1);
        kv->version = 1;
    }
    s->version++;

    /* 构建完整 desired JSON 并同步到 DB */
    cJSON *desired_json = cJSON_CreateObject();
    for (int i = 0; i < s->desired_n; i++) {
        cJSON *val = cJSON_Parse(s->desired[i].value);
        if (val) {
            cJSON_AddItemToObject(desired_json, s->desired[i].key, val);
        } else {
            cJSON_AddStringToObject(desired_json, s->desired[i].key, s->desired[i].value);
        }
    }
    char *desired_str = cJSON_PrintUnformatted(desired_json);
    cJSON_Delete(desired_json);

    /* 重算 delta: desired 中与 reported 不同的项 */
    cJSON *delta_json = cJSON_CreateObject();
    for (int i = 0; i < s->desired_n; i++) {
        shadow_kv_t *rep = kv_find(s->reported, s->reported_n, s->desired[i].key);
        if (!rep || strcmp(rep->value, s->desired[i].value) != 0) {
            cJSON *val = cJSON_Parse(s->desired[i].value);
            if (val) {
                cJSON_AddItemToObject(delta_json, s->desired[i].key, val);
            } else {
                cJSON_AddStringToObject(delta_json, s->desired[i].key, s->desired[i].value);
            }
        }
    }
    char *delta_str = cJSON_PrintUnformatted(delta_json);
    cJSON_Delete(delta_json);

    pthread_mutex_unlock(&g_lock);

    /* 写入 DB */
    if (desired_str) {
        db_conn_t *conn = db_pool_get();
        if (conn) {
            const char *delta_src = delta_str ? delta_str : "{}";
            size_t dlen = strlen(desired_str);
            size_t rlen = strlen(delta_src);
            size_t cap = (dlen + rlen) * 2 + 512;      /* 按 payload 分配, 避免静态缓冲截断 SQL */
            char *esc_d = malloc(dlen * 2 + 1);
            char *esc_delta = malloc(rlen * 2 + 1);
            char *sql = malloc(cap);
            char esc_id[SQL_ESC_CAP(64)];

            if (esc_d && esc_delta && sql &&
                sql_escape_conn(conn, esc_id, sizeof(esc_id), device_id) == 0 &&
                sql_escape_conn(conn, esc_d, dlen * 2 + 1, desired_str) == 0 &&
                sql_escape_conn(conn, esc_delta, rlen * 2 + 1, delta_src) == 0) {
                /* 先尝试 UPDATE (exec 内部可能重连, 影响行数重新取句柄) */
                snprintf(sql, cap,
                    "UPDATE device_shadows SET desired='%s', delta='%s', version=version+1 WHERE device_id='%s'",
                    esc_d, esc_delta, esc_id);
                unsigned long long affected = 0;
                if (db_pool_exec(conn, sql) == 0)
                    affected = (unsigned long long)mysql_affected_rows((MYSQL *)db_pool_get_mysql(conn));
                if (affected == 0) {
                    /* UPDATE 失败或无匹配行，执行 INSERT */
                    snprintf(sql, cap,
                        "INSERT INTO device_shadows (product_key, device_id, desired, delta) "
                        "SELECT product_key, device_id, '%s', '%s' FROM devices WHERE device_id='%s' LIMIT 1",
                        esc_d, esc_delta, esc_id);
                    db_pool_exec(conn, sql);
                }
            } else {
                LOG_WARN("shadow_write_desired: parameter too long or alloc failed (%s)", device_id);
            }
            free(esc_d);
            free(esc_delta);
            free(sql);
            db_pool_put(conn);
        }
        free(desired_str);
    }
    free(delta_str);
    return 0;
}

int shadow_update_reported(const char *device_id, const char *key, const char *value) {
    if (!device_id || !key || !value) return -1;
    pthread_mutex_lock(&g_lock);
    device_shadow_t *s = shadow_get_or_create(device_id);
    if (!s) { pthread_mutex_unlock(&g_lock); return -1; }
    shadow_kv_t *kv = kv_find(s->reported, s->reported_n, key);
    if (kv) {
        strncpy(kv->value, value, SHADOW_VAL_LEN - 1);
        kv->version++;
    } else if (s->reported_n < SHADOW_MAX_KVS) {
        kv = &s->reported[s->reported_n++];
        strncpy(kv->key, key, SHADOW_KEY_LEN - 1);
        strncpy(kv->value, value, SHADOW_VAL_LEN - 1);
        kv->version = 1;
    }
    s->version++;

    /* 重算 delta: reported 更新后，检查是否消除了差异 */
    cJSON *delta_json = cJSON_CreateObject();
    for (int i = 0; i < s->desired_n; i++) {
        shadow_kv_t *rep = kv_find(s->reported, s->reported_n, s->desired[i].key);
        if (!rep || strcmp(rep->value, s->desired[i].value) != 0) {
            cJSON *val = cJSON_Parse(s->desired[i].value);
            if (val) {
                cJSON_AddItemToObject(delta_json, s->desired[i].key, val);
            } else {
                cJSON_AddStringToObject(delta_json, s->desired[i].key, s->desired[i].value);
            }
        }
    }
    char *delta_str = cJSON_PrintUnformatted(delta_json);
    cJSON_Delete(delta_json);
    pthread_mutex_unlock(&g_lock);

    /* 写入 DB delta */
    if (delta_str) {
        db_conn_t *conn = db_pool_get();
        if (conn) {
            size_t rlen = strlen(delta_str);
            char *esc = malloc(rlen * 2 + 1);
            char esc_id[SQL_ESC_CAP(64)];
            if (esc &&
                sql_escape_conn(conn, esc, rlen * 2 + 1, delta_str) == 0 &&
                sql_escape_conn(conn, esc_id, sizeof(esc_id), device_id) == 0) {
                char sql[1024];
                snprintf(sql, sizeof(sql),
                    "UPDATE device_shadows SET delta='%s', version=version+1 WHERE device_id='%s'",
                    esc, esc_id);
                db_pool_exec(conn, sql);
            } else {
                LOG_WARN("shadow_write_delta: parameter too long or alloc failed (%s)", device_id);
            }
            free(esc);
            db_pool_put(conn);
        }
        free(delta_str);
    }
    return 0;
}

int shadow_compute_delta(const char *device_id, shadow_kv_t *out_delta, int max_n) {
    if (!device_id || !out_delta || max_n <= 0) return 0;
    pthread_mutex_lock(&g_lock);
    device_shadow_t *s = NULL;
    for (int i = 0; i < g_shadow_count; i++)
        if (strcmp(g_shadows[i].device_id, device_id) == 0) { s = &g_shadows[i]; break; }
    int n = 0;
    if (s) {
        for (int i = 0; i < s->desired_n && n < max_n; i++) {
            shadow_kv_t *rep = kv_find(s->reported, s->reported_n, s->desired[i].key);
            if (!rep || strcmp(rep->value, s->desired[i].value) != 0) {
                out_delta[n++] = s->desired[i];
            }
        }
    }
    pthread_mutex_unlock(&g_lock);
    return n;
}

const device_shadow_t *shadow_find(const char *device_id) {
    if (!device_id) return NULL;
    pthread_mutex_lock(&g_lock);
    for (int i = 0; i < g_shadow_count; i++)
        if (strcmp(g_shadows[i].device_id, device_id) == 0) {
            const device_shadow_t *r = &g_shadows[i];
            pthread_mutex_unlock(&g_lock);
            return r;
        }
    pthread_mutex_unlock(&g_lock);
    return NULL;
}
