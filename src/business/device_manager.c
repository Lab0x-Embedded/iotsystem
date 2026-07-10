/**
 * @file device_manager.c
 *
 * P4 设备注册/认证/状态 — 内存索引 + MySQL 持久化
 *
 * 字段对齐: 见 deploy/sql/init.sql 的 `devices` 表
 */
#include "business/device_manager.h"
#include "data/db_pool.h"
#include <mysql.h>
#include "common/log.h"

#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <time.h>

#define MAX_DEVICES 1024

static device_info_t g_devices[MAX_DEVICES];
static int            g_device_count = 0;
static pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;

/** 将 DB status 字符串映射到 device_state_t */
static device_state_t state_from_str(const char *s) {
    if (!s) return DEV_STATE_REGISTERED;
    if (strcmp(s, "active") == 0)      return DEV_STATE_ACTIVE;
    if (strcmp(s, "disabled") == 0)    return DEV_STATE_DISABLED;
    if (strcmp(s, "decommissioned") == 0) return DEV_STATE_DISABLED;
    return DEV_STATE_REGISTERED;
}

int device_manager_init(void) {
    pthread_mutex_lock(&g_lock);
    memset(g_devices, 0, sizeof(g_devices));
    g_device_count = 0;
    pthread_mutex_unlock(&g_lock);

    // 从数据库加载设备
    // 列顺序: device_id, device_name, product_key, device_type, device_secret,
    //         group_id, status, online, last_online, updated_at
    db_conn_t *conn = db_pool_get();
    if (conn) {
        const char *sql =
            "SELECT device_id, device_name, product_key, "
            "COALESCE(device_type,''), device_secret, "
            "IFNULL(group_id,0), status, COALESCE(online,0), "
            "UNIX_TIMESTAMP(last_online), UNIX_TIMESTAMP(updated_at) "
            "FROM devices";
        void *result = db_pool_query(conn, sql);
        if (result) {
            MYSQL_RES *res = (MYSQL_RES *)result;
            MYSQL_ROW row;
            while ((row = mysql_fetch_row(res)) && g_device_count < MAX_DEVICES) {
                device_info_t *d = &g_devices[g_device_count++];
                memset(d, 0, sizeof(*d));
                strncpy(d->device_id, row[0] ? row[0] : "", DEV_ID_LEN - 1);
                strncpy(d->name, row[1] ? row[1] : "", DEV_NAME_LEN - 1);
                strncpy(d->product_key, row[2] ? row[2] : "", DEV_PK_LEN - 1);
                strncpy(d->device_type, row[3] ? row[3] : "", DEV_TYPE_LEN - 1);
                strncpy(d->device_secret, row[4] ? row[4] : "", DEV_SECRET_LEN - 1);
                d->group_id = row[5] ? atoi(row[5]) : 0;
                d->state = state_from_str(row[6]);
                d->online = row[7] ? (atoi(row[7]) != 0) : false;
                d->last_online = row[8] ? (time_t)atoll(row[8]) : 0;
                d->updated_at = row[9] ? (time_t)atoll(row[9]) : 0;
                d->registered_at = d->updated_at; /* 近似 */
            }
            db_pool_free_result(result);
        }
        db_pool_put(conn);
        LOG_INFO("device_manager: loaded %d devices from database", g_device_count);
    } else {
        LOG_WARN("device_manager: no db connection, using memory only");
    }

    LOG_INFO("device_manager initialized (loaded=%d, capacity=%d)", g_device_count, MAX_DEVICES);
    return 0;
}

int device_register(const char *device_id, const char *name,
                    const char *product_key, const char *device_type,
                    const char *device_secret, int group_id) {
    if (!device_id || !product_key) return -1;

    const char *dt = device_type ? device_type : "";
    const char *ds = device_secret ? device_secret : "";

    // 写入数据库
    db_conn_t *conn = db_pool_get();
    if (conn) {
        char sql[1024];
        snprintf(sql, sizeof(sql),
            "INSERT INTO devices "
            "(device_id, device_name, product_key, device_type, device_secret, group_id, status) "
            "VALUES ('%s', '%s', '%s', '%s', '%s', %d, 'registered') "
            "ON DUPLICATE KEY UPDATE device_name='%s', device_type='%s'",
            device_id,
            name ? name : "",
            product_key,
            dt,
            ds,
            group_id,
            name ? name : "",
            dt);

        if (db_pool_exec(conn, sql) != 0) {
            MYSQL *mysql = (MYSQL *)db_pool_get_mysql(conn);
            LOG_ERROR("device_register: db insert failed for %s, error: %s",
                      device_id, mysql ? mysql_error(mysql) : "unknown");
            db_pool_put(conn);
            return -1;
        }
        db_pool_put(conn);
        LOG_INFO("device registered in DB: id=%s", device_id);
    }

    // 写入内存索引
    pthread_mutex_lock(&g_lock);
    if (g_device_count >= MAX_DEVICES) {
        pthread_mutex_unlock(&g_lock);
        LOG_WARN("device_register: memory full (max=%d)", MAX_DEVICES);
        return -1;
    }
    device_info_t *d = &g_devices[g_device_count++];
    memset(d, 0, sizeof(*d));
    strncpy(d->device_id, device_id, DEV_ID_LEN - 1);
    if (name) strncpy(d->name, name, DEV_NAME_LEN - 1);
    strncpy(d->product_key, product_key, DEV_PK_LEN - 1);
    strncpy(d->device_type, dt, DEV_TYPE_LEN - 1);
    strncpy(d->device_secret, ds, DEV_SECRET_LEN - 1);
    d->group_id = group_id;
    d->state = DEV_STATE_REGISTERED;
    d->online = false;
    d->registered_at = time(NULL);
    d->last_active = 0;
    d->report_count = 0;
    pthread_mutex_unlock(&g_lock);

    LOG_INFO("device registered: id=%s product=%s", device_id, product_key);
    return 0;
}

void device_manager_online(const char *device_id, uint16_t keepalive) {
    if (!device_id) return;

    db_conn_t *conn = db_pool_get();
    if (conn) {
        char sql[256];
        snprintf(sql, sizeof(sql),
            "UPDATE devices SET status='active', online=1, last_online=NOW() WHERE device_id='%s'",
            device_id);
        db_pool_exec(conn, sql);
        db_pool_put(conn);
    }

    pthread_mutex_lock(&g_lock);
    for (int i = 0; i < g_device_count; i++) {
        if (strcmp(g_devices[i].device_id, device_id) == 0) {
            g_devices[i].state = DEV_STATE_ACTIVE;
            g_devices[i].online = true;
            g_devices[i].keepalive = keepalive;
            g_devices[i].last_active = time(NULL);
            g_devices[i].last_online = g_devices[i].last_active;
            pthread_mutex_unlock(&g_lock);
            LOG_INFO("device online: id=%s keepalive=%u", device_id, keepalive);
            return;
        }
    }
    pthread_mutex_unlock(&g_lock);
}

void device_manager_heartbeat(const char *device_id) {
    if (!device_id) return;
    pthread_mutex_lock(&g_lock);
    for (int i = 0; i < g_device_count; i++) {
        if (strcmp(g_devices[i].device_id, device_id) == 0) {
            g_devices[i].last_active = time(NULL);
            g_devices[i].report_count++;
            if (!g_devices[i].online) {
                g_devices[i].online = true;
                g_devices[i].last_online = g_devices[i].last_active;
            }
            break;
        }
    }
    pthread_mutex_unlock(&g_lock);
}

void device_manager_offline(const char *device_id) {
    if (!device_id) return;

    db_conn_t *conn = db_pool_get();
    if (conn) {
        char sql[256];
        snprintf(sql, sizeof(sql),
            "UPDATE devices SET online=0 WHERE device_id='%s'",
            device_id);
        db_pool_exec(conn, sql);
        db_pool_put(conn);
    }

    pthread_mutex_lock(&g_lock);
    for (int i = 0; i < g_device_count; i++) {
        if (strcmp(g_devices[i].device_id, device_id) == 0) {
            g_devices[i].online = false;
            break;
        }
    }
    pthread_mutex_unlock(&g_lock);
}

const device_info_t *device_manager_find(const char *device_id) {
    if (!device_id) return NULL;
    pthread_mutex_lock(&g_lock);
    for (int i = 0; i < g_device_count; i++) {
        if (strcmp(g_devices[i].device_id, device_id) == 0) {
            const device_info_t *r = &g_devices[i];
            pthread_mutex_unlock(&g_lock);
            return r;
        }
    }
    pthread_mutex_unlock(&g_lock);
    return NULL;
}

void device_manager_tick(time_t now) {
    pthread_mutex_lock(&g_lock);
    for (int i = 0; i < g_device_count; i++) {
        if (g_devices[i].online &&
            g_devices[i].last_active > 0 &&
            g_devices[i].keepalive > 0 &&
            now - g_devices[i].last_active > g_devices[i].keepalive * 1.5) {
            g_devices[i].online = false;
            LOG_INFO("device timeout: id=%s", g_devices[i].device_id);
        }
    }
    pthread_mutex_unlock(&g_lock);
}

int device_manager_online_count(void) {
    int count = 0;
    pthread_mutex_lock(&g_lock);
    for (int i = 0; i < g_device_count; i++) {
        if (g_devices[i].online)
            count++;
    }
    pthread_mutex_unlock(&g_lock);
    return count;
}

int device_manager_decommission(const char *device_id) {
    if (!device_id) return -1;

    db_conn_t *conn = db_pool_get();
    if (conn) {
        char sql[256];
        snprintf(sql, sizeof(sql),
            "UPDATE devices SET status='decommissioned', online=0 WHERE device_id='%s'",
            device_id);
        db_pool_exec(conn, sql);
        db_pool_put(conn);
    }

    pthread_mutex_lock(&g_lock);
    for (int i = 0; i < g_device_count; i++) {
        if (strcmp(g_devices[i].device_id, device_id) == 0) {
            g_devices[i].state = DEV_STATE_DISABLED;
            g_devices[i].online = false;
            pthread_mutex_unlock(&g_lock);
            LOG_INFO("device decommissioned: id=%s", device_id);
            return 0;
        }
    }
    pthread_mutex_unlock(&g_lock);
    return -1;
}

int device_manager_get_all(const device_info_t **devices, int *count) {
    if (!devices || !count) return -1;
    pthread_mutex_lock(&g_lock);
    *devices = g_devices;
    *count = g_device_count;
    pthread_mutex_unlock(&g_lock);
    return 0;
}

int device_manager_total_count(void) {
    pthread_mutex_lock(&g_lock);
    int n = g_device_count;
    pthread_mutex_unlock(&g_lock);
    return n;
}
