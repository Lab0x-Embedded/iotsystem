/**
 * @file shadow_manager.c
 *
 * P4 设备影子 — 内存实现
 */
#include "business/shadow_manager.h"
#include "common/log.h"

#include <stdlib.h>
#include <string.h>
#include <pthread.h>

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
    pthread_mutex_unlock(&g_lock);
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
    pthread_mutex_unlock(&g_lock);
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
