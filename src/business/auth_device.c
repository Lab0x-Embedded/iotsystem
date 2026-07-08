/**
 * @file auth_device.c
 *
 * P4 设备认证 — 当前采用内存查找; MySQL 持久化留由 db_pool 接入.
 */
#include "business/auth_device.h"
#include "common/log.h"

#include <stdlib.h>
#include <string.h>
#include <pthread.h>

/* 简易内存设备凭证表, 作为 demo fallback */
typedef struct {
    char product_key[33];
    char device_id[65];
    char secret[128];
} auth_entry_t;

#define MAX_AUTH_ENTRIES 256
static auth_entry_t g_auth[MAX_AUTH_ENTRIES];
static int          g_auth_count = 0;
static int          g_initialized = 0;

static int auth_add_credential(const char *pk, const char *did, const char *secret) {
    if (g_auth_count >= MAX_AUTH_ENTRIES) return -1;
    auth_entry_t *e = &g_auth[g_auth_count++];
    strncpy(e->product_key, pk, 32);
    strncpy(e->device_id, did, 64);
    strncpy(e->secret, secret, 127);
    return 0;
}
int auth_device_init(void) {
    if (g_initialized) return 0;
    memset(g_auth, 0, sizeof(g_auth));
    g_auth_count = 0;
    g_initialized = 1;
    /* 注入一条 demo 凭据, 便于集成测试 */
    auth_add_credential("pk_test", "dev_001", "secret_001");
    LOG_INFO("auth_device initialized");
    return 0;
}


int auth_device_verify(const char *product_key,
                       const char *device_id,
                       const char *password) {
    if (!product_key || !device_id || !password) return -1;
    for (int i = 0; i < g_auth_count; i++) {
        if (strcmp(g_auth[i].product_key, product_key) == 0 &&
            strcmp(g_auth[i].device_id, device_id) == 0) {
            /* demo: 密码直接与 secret 比较 */
            if (strcmp(password, g_auth[i].secret) == 0) return 0;
            LOG_WARN("auth failed for %s/%s: bad password", product_key, device_id);
            return -1;
        }
    }
    /* 兜底 demo 凭据, 便于无 MySQL 时压测 */
    if (strcmp(product_key, "pk_test") == 0 &&
        strcmp(device_id, "dev_001") == 0 &&
        strcmp(password, "secret_001") == 0) return 0;
    LOG_WARN("auth failed: unknown device %s/%s", product_key, device_id);
    return -1;
}

void auth_device_shutdown(void) {
    g_initialized = 0;
    g_auth_count = 0;
    LOG_INFO("auth_device shutdown");
}
