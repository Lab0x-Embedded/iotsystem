/**
 * @file device_manager.c
 *
 * P4 设备注册/认证/状态 — 内存索引实现
 */
#include "business/device_manager.h"
#include "common/log.h"

#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#define MAX_DEVICES 1024

static device_info_t g_devices[MAX_DEVICES];
static int            g_device_count = 0;
static pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;

int device_manager_init(void) {
    pthread_mutex_lock(&g_lock);
    memset(g_devices, 0, sizeof(g_devices));
    g_device_count = 0;
    pthread_mutex_unlock(&g_lock);
    LOG_INFO("device_manager initialized (capacity=%d)", MAX_DEVICES);
    return 0;
}

int device_register(const char *device_id, const char *name,
                    const char *product_key, const char *group_id) {
    if (!device_id || !product_key) return -1;
    pthread_mutex_lock(&g_lock);
    if (g_device_count >= MAX_DEVICES) {
        pthread_mutex_unlock(&g_lock);
        LOG_WARN("device_register: table full (max=%d)", MAX_DEVICES);
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
        device_info_t *d = &g_devices[i];
        if (d->state != DEV_STATE_ONLINE) continue;
        if (d->keepalive == 0) continue;
        time_t threshold = (time_t)(d->keepalive * 1.5);
        if (now - d->last_active > threshold) {
            d->state = DEV_STATE_OFFLINE;
            LOG_INFO("device timeout -> offline: id=%s", d->device_id);
        }
    }
    pthread_mutex_unlock(&g_lock);
}

int device_manager_online_count(void) {
    int n = 0;
    pthread_mutex_lock(&g_lock);
    for (int i = 0; i < g_device_count; i++)
        if (g_devices[i].state == DEV_STATE_ONLINE) n++;
    pthread_mutex_unlock(&g_lock);
    return n;
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
