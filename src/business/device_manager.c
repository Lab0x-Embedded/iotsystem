/**
 * @file device_manager.c
 *
 * P4 设备注册/认证/状态 — 设备全生命周期管理 (MySQL 持久化, 无内存缓存)
 *
 * 设计:
 *   - 生命周期态 (lifecycle state): REGISTERED -> ACTIVE -> DISABLED
 *   - 瞬时在线态 (online flag): 独立于 lifecycle state，由心跳/离线事件和 DB `online` 列维护
 *   - 表结构对齐: 见 deploy/sql/init.sql 的 `devices` 表
 *   - 心跳保活: 超过 keepalive*1.5 无活动判定离线 (online=false, state 不变)
 *   - 无内存索引: 每次查询直接查 DB, 设备数据直接在 DB 中管理
 */
#include "business/device_manager.h"
#include "data/db_pool.h"
#include "common/log.h"

#include <mysql.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/** 将 DB status 字符串映射到 device_state_t */
static device_state_t state_from_str(const char *s) {
    if (!s) return DEV_STATE_REGISTERED;
    if (strcmp(s, "active") == 0)      return DEV_STATE_ACTIVE;
    if (strcmp(s, "disabled") == 0)    return DEV_STATE_DISABLED;
    if (strcmp(s, "decommissioned") == 0) return DEV_STATE_DISABLED;
    return DEV_STATE_REGISTERED;
}

/* DB columns in canonical SELECT order:
 *   0 device_id
 *   1 device_name
 *   2 product_key
 *   3 device_type
 *   4 device_secret
 *   5 group_id
 *   6 status
 *   7 online
 *   8 last_online
 *   9 updated_at
 */
static const char *g_select_base =
    "SELECT device_id, device_name, product_key, "
    "COALESCE(device_type,''), device_secret, "
    "IFNULL(group_id,0), status, COALESCE(online,0), "
    "UNIX_TIMESTAMP(last_online), "
    "UNIX_TIMESTAMP(updated_at) "
    "FROM devices";

static device_info_t *query_exec(const char *sql, int *out_count) {
    if (!out_count) return NULL;
    *out_count = 0;

    db_conn_t *conn = db_pool_get();
    if (!conn) {
        LOG_ERROR("device_manager: no db connection for query");
        return NULL;
    }

    void *result = db_pool_query(conn, sql);
    if (!result) {
        db_pool_put(conn);
        return NULL;
    }

    MYSQL_RES *res = (MYSQL_RES *)result;
    unsigned long long nrows = mysql_num_rows(res);
    if (nrows == 0) {
        db_pool_free_result(result);
        db_pool_put(conn);
        return NULL;
    }

    int count = (int)nrows;
    device_info_t *arr = calloc(count, sizeof(device_info_t));
    if (!arr) {
        db_pool_free_result(result);
        db_pool_put(conn);
        return NULL;
    }

    MYSQL_ROW row;
    int i = 0;
    while ((row = mysql_fetch_row(res)) && i < count) {
        device_info_t *d = &arr[i];
        memset(d, 0, sizeof(*d));
        strncpy(d->device_id,     row[0] ? row[0] : "", DEV_ID_LEN - 1);
        strncpy(d->name,          row[1] ? row[1] : "", DEV_NAME_LEN - 1);
        strncpy(d->product_key,   row[2] ? row[2] : "", DEV_PK_LEN - 1);
        strncpy(d->device_type,   row[3] ? row[3] : "", DEV_TYPE_LEN - 1);
        strncpy(d->device_secret, row[4] ? row[4] : "", DEV_SECRET_LEN - 1);
        d->group_id    = row[5] ? atoi(row[5]) : 0;
        d->state       = state_from_str(row[6]);
        d->online      = row[7] ? (atoi(row[7]) != 0) : false;
        d->last_online = row[8] ? (time_t)atoll(row[8]) : 0;
        d->updated_at  = row[9] ? (time_t)atoll(row[9]) : 0;
        d->last_active = d->last_online;
        d->registered_at = d->updated_at; /* 近似 */
        i++;
    }

    db_pool_free_result(result);
    db_pool_put(conn);

    *out_count = i;
    if (i == 0) { free(arr); return NULL; }
    return arr;
}

/* 构造 WHERE group_id= 的查询 */
static char *build_group_sql(int group_id) {
    /* "SELECT ... FROM devices WHERE group_id=123" */
    size_t len = strlen(g_select_base) + 64;
    char *sql = malloc(len);
    if (sql)
        snprintf(sql, len, "%s WHERE group_id=%d", g_select_base, group_id);
    return sql;
}

int device_manager_init(void) {
    /* 无内存索引, 仅验证 DB 可达 */
    db_conn_t *conn = db_pool_get();
    if (conn) {
        LOG_INFO("device_manager initialized (DB-backed, no in-memory cache)");
        db_pool_put(conn);
    } else {
        LOG_WARN("device_manager: no db connection");
    }
    return 0;
}

int device_register(const char *device_id, const char *name,
                    const char *product_key, const char *device_type,
                    const char *device_secret, int group_id) {
    if (!device_id || !product_key) return -1;

    const char *dt = device_type ? device_type : "";
    const char *ds = device_secret ? device_secret : "";

    db_conn_t *conn = db_pool_get();
    if (!conn) return -1;

    /* 转义防止 SQL 注入 */
    char esc_id[DEV_ID_LEN * 2 + 1], esc_name[DEV_NAME_LEN * 2 + 1];
    char esc_pk[DEV_PK_LEN * 2 + 1], esc_dt[DEV_TYPE_LEN * 2 + 1];
    char esc_ds[DEV_SECRET_LEN * 2 + 1];
    MYSQL *mysql = (MYSQL *)db_pool_get_mysql(conn);

    mysql_real_escape_string(mysql, esc_id, device_id, strlen(device_id));
    mysql_real_escape_string(mysql, esc_name, name ? name : "", strlen(name ? name : ""));
    mysql_real_escape_string(mysql, esc_pk, product_key, strlen(product_key));
    mysql_real_escape_string(mysql, esc_dt, dt, strlen(dt));
    mysql_real_escape_string(mysql, esc_ds, ds, strlen(ds));

    char sql[2048];
    snprintf(sql, sizeof(sql),
        "INSERT INTO devices "
        "(device_id, device_name, product_key, device_type, device_secret, group_id, status) "
        "VALUES ('%s', '%s', '%s', '%s', '%s', %d, 'registered') "
        "ON DUPLICATE KEY UPDATE device_name='%s', device_type='%s'",
        esc_id, esc_name, esc_pk, esc_dt, esc_ds,
        group_id, esc_name, esc_dt);

    int rc = db_pool_exec(conn, sql);
    db_pool_put(conn);

    if (rc != 0) {
        LOG_ERROR("device_register: db insert failed for %s", device_id);
        return -1;
    }

    LOG_INFO("device registered: id=%s pk=%s group=%d", device_id, product_key, group_id);
    return 0;
}

void device_manager_online(const char *device_id, uint16_t keepalive) {
    (void)keepalive;
    if (!device_id) return;

    db_conn_t *conn = db_pool_get();
    if (!conn) return;

    char sql[256];
    snprintf(sql, sizeof(sql),
        "UPDATE devices SET online=1, last_online=NOW() "
        "WHERE device_id='%s'",
        device_id);
    db_pool_exec(conn, sql);
    db_pool_put(conn);

    LOG_DEBUG("device online: id=%s", device_id);
}

void device_manager_heartbeat(const char *device_id) {
    if (!device_id) return;

    db_conn_t *conn = db_pool_get();
    if (!conn) return;

    char sql[256];
    snprintf(sql, sizeof(sql),
        "UPDATE devices SET last_online=NOW() WHERE device_id='%s'",
        device_id);
    db_pool_exec(conn, sql);
    db_pool_put(conn);

    LOG_DEBUG("device heartbeat: id=%s", device_id);
}

void device_manager_offline(const char *device_id) {
    if (!device_id) return;

    db_conn_t *conn = db_pool_get();
    if (!conn) return;

    char sql[256];
    snprintf(sql, sizeof(sql),
        "UPDATE devices SET online=0, last_online=NOW() WHERE device_id='%s'",
        device_id);
    db_pool_exec(conn, sql);
    db_pool_put(conn);

    LOG_DEBUG("device offline: id=%s", device_id);
}

const device_info_t *device_manager_find(const char *device_id) {
    if (!device_id) return NULL;

    size_t len = strlen(g_select_base) + strlen(device_id) + 64;
    char *sql = malloc(len);
    if (!sql) return NULL;
    snprintf(sql, len, "%s WHERE device_id='%s'", g_select_base, device_id);

    int count = 0;
    device_info_t *arr = query_exec(sql, &count);
    free(sql);

    if (!arr || count == 0) return NULL;

    /* 复制单个结果到静态缓冲区, 调用者不可修改也不可 free */
    static device_info_t s_single;
    s_single = arr[0];
    free(arr);
    return &s_single;
}

void device_manager_tick(time_t now) {
    if (now == 0) now = time(NULL);

    /* 查出在线设备, 逐个判断是否超过 60 秒无心跳 */
    const char *sql =
        "SELECT device_id, UNIX_TIMESTAMP(last_online) "
        "FROM devices WHERE online=1";

    db_conn_t *conn = db_pool_get();
    if (!conn) return;

    void *result = db_pool_query(conn, sql);
    if (result) {
        MYSQL_RES *res = (MYSQL_RES *)result;
        MYSQL_ROW row;
        while ((row = mysql_fetch_row(res))) {
            const char *id = row[0];
            time_t last = row[1] ? (time_t)atoll(row[1]) : 0;
            if (last > 0 && now - last > 60) {
                char upd[256];
                snprintf(upd, sizeof(upd),
                    "UPDATE devices SET online=0 WHERE device_id='%s'", id);
                db_pool_exec(conn, upd);
                LOG_INFO("device timeout: id=%s", id);
            }
        }
        db_pool_free_result(result);
    }
    db_pool_put(conn);
}

int device_manager_online_count(void) {
    db_conn_t *conn = db_pool_get();
    if (!conn) return 0;

    void *result = db_pool_query(conn,
        "SELECT COUNT(*) FROM devices WHERE online=1");
    int count = 0;
    if (result) {
        MYSQL_RES *res = (MYSQL_RES *)result;
        MYSQL_ROW row = mysql_fetch_row(res);
        if (row && row[0]) count = atoi(row[0]);
        db_pool_free_result(result);
    }
    db_pool_put(conn);
    return count;
}

int device_manager_decommission(const char *device_id) {
    if (!device_id) return -1;

    db_conn_t *conn = db_pool_get();
    if (!conn) return -1;

    char sql[256];
    snprintf(sql, sizeof(sql),
        "UPDATE devices SET status='decommissioned', online=0 WHERE device_id='%s'",
        device_id);
    int rc = db_pool_exec(conn, sql);
    db_pool_put(conn);

    if (rc == 0) {
        LOG_INFO("device decommissioned: id=%s", device_id);
        return 0;
    }
    return -1;
}

/** 获取所有设备列表.
 *  @return 动态分配数组, 调用者必须 free() */
int device_manager_get_all(const device_info_t **devices, int *count) {
    if (!devices || !count) return -1;
    *devices = query_exec(g_select_base, count);
    return 0;
}

/** 获取设备总数. */
int device_manager_total_count(void) {
    db_conn_t *conn = db_pool_get();
    if (!conn) return 0;

    void *result = db_pool_query(conn, "SELECT COUNT(*) FROM devices");
    int count = 0;
    if (result) {
        MYSQL_RES *res = (MYSQL_RES *)result;
        MYSQL_ROW row = mysql_fetch_row(res);
        if (row && row[0]) count = atoi(row[0]);
        db_pool_free_result(result);
    }
    db_pool_put(conn);
    return count;
}

/** 按 group_id 获取设备列表.
 *  @return 动态分配数组, 调用者必须 free() */
int device_manager_get_by_group(int group_id, const device_info_t **devices, int *count) {
    if (!devices || !count) return -1;

    char *sql = build_group_sql(group_id);
    if (!sql) return -1;

    *devices = query_exec(sql, count);
    free(sql);
    return 0;
}

/** 更新设备分组. */
int device_manager_update_group(const char *device_id, int group_id) {
    if (!device_id) return -1;

    db_conn_t *conn = db_pool_get();
    if (!conn) return -1;

    char sql[256];
    snprintf(sql, sizeof(sql),
        "UPDATE devices SET group_id=%d, updated_at=NOW() WHERE device_id='%s'",
        group_id, device_id);
    int rc = db_pool_exec(conn, sql);
    db_pool_put(conn);

    return rc == 0 ? 0 : -1;
}

/** 更新设备名称. */
int device_manager_update_name(const char *device_id, const char *name) {
    if (!device_id || !name) return -1;

    db_conn_t *conn = db_pool_get();
    if (!conn) return -1;

    char esc[DEV_NAME_LEN * 2 + 1];
    MYSQL *mysql = (MYSQL *)db_pool_get_mysql(conn);
    mysql_real_escape_string(mysql, esc, name, strlen(name));

    char sql[512];
    snprintf(sql, sizeof(sql),
        "UPDATE devices SET device_name='%s', updated_at=NOW() WHERE device_id='%s'",
        esc, device_id);
    int rc = db_pool_exec(conn, sql);
    db_pool_put(conn);

    return rc == 0 ? 0 : -1;
}
