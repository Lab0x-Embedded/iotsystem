/**
 * @file device_manager.c
 *
 * P4 设备注册/认证/状态 — 内存索引 + MySQL 持久化
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

int device_manager_init(void) {
    pthread_mutex_lock(&g_lock);
    memset(g_devices, 0, sizeof(g_devices));
    g_device_count = 0;
    pthread_mutex_unlock(&g_lock);
    
    // 从数据库加载设备
    db_conn_t *conn = db_pool_get();
    if (conn) {
        const char *sql = "SELECT device_id, device_name, product_key, IFNULL(group_id,0), status FROM devices";
        void *result = db_pool_query(conn, sql);
        if (result) {
            MYSQL_RES *res = (MYSQL_RES *)result;
            MYSQL_ROW row;
            while ((row = mysql_fetch_row(res)) && g_device_count < MAX_DEVICES) {
                device_info_t *d = &g_devices[g_device_count++];
                strncpy(d->device_id, row[0], DEV_ID_LEN - 1);
                strncpy(d->name, row[1] ? row[1] : "", DEV_NAME_LEN - 1);
                strncpy(d->product_key, row[2], 32);
                strncpy(d->group_id, row[3] ? row[3] : "", DEV_ID_LEN - 1);
                
                if (row[4]) {
                    if (strcmp(row[4], "active") == 0) d->state = DEV_STATE_ACTIVE;
                    else if (strcmp(row[4], "disabled") == 0) d->state = DEV_STATE_DISABLED;
                    else d->state = DEV_STATE_REGISTERED;
                } else {
                    d->state = DEV_STATE_REGISTERED;
                }
                d->last_active = 0;
                d->report_count = 0;
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
                    const char *product_key, const char *group_id) {
    if (!device_id || !product_key) return -1;
    
    // 写入数据库
    db_conn_t *conn = db_pool_get();
    if (conn) {
        char sql[512];
        // 使用正确的字段名 device_name，group_id转为整数
        int gid = group_id ? atoi(group_id) : 0;
        snprintf(sql, sizeof(sql),
            "INSERT INTO devices (device_id, device_name, product_key, group_id, status) "
            "VALUES ('%s', '%s', '%s', %d, 'registered') "
            "ON DUPLICATE KEY UPDATE device_name='%s'",
            device_id,
            name ? name : "",
            product_key,
            gid,
            name ? name : "");
        
        if (db_pool_exec(conn, sql) != 0) {
            // 获取MySQL错误信息
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
    strncpy(d->device_id, device_id, DEV_ID_LEN - 1);
    if (name) strncpy(d->name, name, DEV_NAME_LEN - 1);
    strncpy(d->product_key, product_key, 32);
    if (group_id) strncpy(d->group_id, group_id, DEV_ID_LEN - 1);
    d->state = DEV_STATE_REGISTERED;
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
            g_devices[i].state = DEV_STATE_ONLINE;
            g_devices[i].keepalive = keepalive;
            g_devices[i].last_active = time(NULL);
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
            if (g_devices[i].state == DEV_STATE_OFFLINE)
                g_devices[i].state = DEV_STATE_ONLINE;
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
            if (g_devices[i].state == DEV_STATE_ONLINE)
                g_devices[i].state = DEV_STATE_OFFLINE;
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
        if (g_devices[i].state == DEV_STATE_ONLINE &&
            g_devices[i].last_active > 0 &&
            now - g_devices[i].last_active > g_devices[i].keepalive * 1.5) {
            g_devices[i].state = DEV_STATE_OFFLINE;
            LOG_INFO("device timeout: id=%s", g_devices[i].device_id);
        }
    }
    pthread_mutex_unlock(&g_lock);
}

int device_manager_online_count(void) {
    int count = 0;
    pthread_mutex_lock(&g_lock);
    for (int i = 0; i < g_device_count; i++) {
        if (g_devices[i].state == DEV_STATE_ONLINE)
            count++;
    }
    pthread_mutex_unlock(&g_lock);
    return count;
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
