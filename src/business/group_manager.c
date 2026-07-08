/**
 * @file group_manager.c
 *
 * P4 设备分组 — 内存实现
 */
#include "business/group_manager.h"
#include "common/log.h"

#include <stdlib.h>
#include <string.h>
#include <pthread.h>

static device_group_t g_groups[MAX_GROUPS];
static int             g_count = 0;
static pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;

int group_manager_init(void) {
    pthread_mutex_lock(&g_lock);
    memset(g_groups, 0, sizeof(g_groups));
    g_count = 0;
    pthread_mutex_unlock(&g_lock);
    LOG_INFO("group_manager initialized (capacity=%d)", MAX_GROUPS);
    return 0;
}

int group_create(const char *group_id, const char *name, const char *parent_id) {
    if (!group_id || !name) return -1;
    pthread_mutex_lock(&g_lock);
    if (g_count >= MAX_GROUPS) { pthread_mutex_unlock(&g_lock); return -1; }
    for (int i = 0; i < g_count; i++)
        if (strcmp(g_groups[i].group_id, group_id) == 0) {
            pthread_mutex_unlock(&g_lock); return -1;
        }
    device_group_t *g = &g_groups[g_count++];
    strncpy(g->group_id, group_id, GROUP_ID_LEN - 1);
    strncpy(g->name, name, GROUP_NAME_LEN - 1);
    if (parent_id) strncpy(g->parent_id, parent_id, GROUP_ID_LEN - 1);
    g->created_at = (uint64_t)time(NULL);
    g->device_count = 0;
    pthread_mutex_unlock(&g_lock);
    LOG_INFO("group created: %s (%s)", group_id, name);
    return 0;
}

int group_remove(const char *group_id) {
    if (!group_id) return -1;
    pthread_mutex_lock(&g_lock);
    for (int i = 0; i < g_count; i++) {
        if (strcmp(g_groups[i].group_id, group_id) == 0) {
            if (g_groups[i].device_count > 0) { pthread_mutex_unlock(&g_lock); return -1; }
            /* 检查是否有子节点 */
            for (int j = 0; j < g_count; j++) {
                if (g_groups[j].parent_id[0] &&
                    strcmp(g_groups[j].parent_id, group_id) == 0) {
                    pthread_mutex_unlock(&g_lock); return -1;
                }
            }
            g_groups[i] = g_groups[--g_count];
            pthread_mutex_unlock(&g_lock);
            LOG_INFO("group removed: %s", group_id);
            return 0;
        }
    }
    pthread_mutex_unlock(&g_lock);
    return -1;
}

const device_group_t *group_find(const char *group_id) {
    if (!group_id) return NULL;
    pthread_mutex_lock(&g_lock);
    for (int i = 0; i < g_count; i++)
        if (strcmp(g_groups[i].group_id, group_id) == 0) {
            const device_group_t *r = &g_groups[i];
            pthread_mutex_unlock(&g_lock);
            return r;
        }
    pthread_mutex_unlock(&g_lock);
    return NULL;
}

int group_list_roots(const device_group_t **out, int max_n) {
    if (!out || max_n <= 0) return 0;
    pthread_mutex_lock(&g_lock);
    int n = 0;
    for (int i = 0; i < g_count && n < max_n; i++)
        if (g_groups[i].parent_id[0] == '\0') out[n++] = &g_groups[i];
    pthread_mutex_unlock(&g_lock);
    return n;
}

int group_list_children(const char *parent_id, const device_group_t **out, int max_n) {
    if (!parent_id || !out || max_n <= 0) return 0;
    pthread_mutex_lock(&g_lock);
    int n = 0;
    for (int i = 0; i < g_count && n < max_n; i++)
        if (strcmp(g_groups[i].parent_id, parent_id) == 0) out[n++] = &g_groups[i];
    pthread_mutex_unlock(&g_lock);
    return n;
}

void group_inc_device(const char *group_id) {
    if (!group_id) return;
    pthread_mutex_lock(&g_lock);
    for (int i = 0; i < g_count; i++)
        if (strcmp(g_groups[i].group_id, group_id) == 0) { g_groups[i].device_count++; break; }
    pthread_mutex_unlock(&g_lock);
}

void group_dec_device(const char *group_id) {
    if (!group_id) return;
    pthread_mutex_lock(&g_lock);
    for (int i = 0; i < g_count; i++)
        if (strcmp(g_groups[i].group_id, group_id) == 0) {
            if (g_groups[i].device_count > 0) g_groups[i].device_count--;
            break;
        }
    pthread_mutex_unlock(&g_lock);
}
