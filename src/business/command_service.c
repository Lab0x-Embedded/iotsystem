/**
 * @file command_service.c
 *
 * P5 指令下行 — 离线队列 + inflight PubACK 等待
 */
#include "command_service.h"
#include "common/log.h"

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <sys/time.h>

/* ----------------------------------------------------------------- */
/* 全局状态                                                           */
/* ----------------------------------------------------------------- */
static inflight_entry_t g_inflight[MAX_INFLIGHT];
static pthread_mutex_t  g_inflight_mutex = PTHREAD_MUTEX_INITIALIZER;

static struct offline_cmd_queue g_offline_queue;
static pthread_mutex_t  g_offline_mutex  = PTHREAD_MUTEX_INITIALIZER;

static pthread_once_t   g_mgr_once = PTHREAD_ONCE_INIT;

/* ----------------------------------------------------------------- */
/* 初始化 (pthread_once)                                              */
/* ----------------------------------------------------------------- */
static void mgr_init_impl(void) {
    memset(g_inflight, 0, sizeof(g_inflight));
    STAILQ_INIT(&g_offline_queue);
    LOG_INFO("command_service initialized");
}

void cmd_mgr_init(void) {
    pthread_once(&g_mgr_once, mgr_init_impl);
}

/* ----------------------------------------------------------------- */
/* Inflight 表                                                        */
/* ----------------------------------------------------------------- */

/* 找到空闲槽并初始化, 返回 inflight_entry 指针. */
static inflight_entry_t *inflight_alloc(int fd, uint16_t pid, const char *cmd_id) {
    pthread_mutex_lock(&g_inflight_mutex);
    for (int i = 0; i < MAX_INFLIGHT; i++) {
        if (!g_inflight[i].used) {
            g_inflight[i].used      = 1;
            g_inflight[i].fd        = fd;
            g_inflight[i].packet_id = pid;
            g_inflight[i].acked     = 0;
            g_inflight[i].sent_at   = time(NULL);
            strncpy(g_inflight[i].cmd_id, cmd_id, CMD_ID_LEN - 1);
            g_inflight[i].cmd_id[CMD_ID_LEN - 1] = '\0';
            pthread_cond_init(&g_inflight[i].cond, NULL);
            pthread_mutex_init(&g_inflight[i].mtx, NULL);
            pthread_mutex_unlock(&g_inflight_mutex);
            return &g_inflight[i];
        }
    }
    pthread_mutex_unlock(&g_inflight_mutex);
    LOG_ERROR("inflight table full");
    return NULL;
}

/* 释放 inflight 条目. */
static void inflight_free_locked(inflight_entry_t *e) {
    pthread_cond_destroy(&e->cond);
    pthread_mutex_destroy(&e->mtx);
    memset(e, 0, sizeof(*e));
}

int cmd_mgr_inflight_wait(int fd, uint16_t packet_id, const char *cmd_id, int timeout_sec) {
    inflight_entry_t *e = inflight_alloc(fd, packet_id, cmd_id);
    if (!e) return -1;

    struct timespec ts;
    struct timeval  tv;
    gettimeofday(&tv, NULL);
    ts.tv_sec  = tv.tv_sec + timeout_sec;
    ts.tv_nsec = tv.tv_usec * 1000L;

    pthread_mutex_lock(&e->mtx);
    while (!e->acked) {
        int rc = pthread_cond_timedwait(&e->cond, &e->mtx, &ts);
        if (rc == ETIMEDOUT) {
            LOG_WARN("inflight timeout fd=%d pid=%u cmd=%s", fd, packet_id, cmd_id);
            break;
        }
        if (rc != 0) {
            LOG_WARN("cond_wait err=%d fd=%d pid=%u", rc, fd, packet_id);
            break;
        }
    }
    int result = e->acked ? 1 : 0;
    pthread_mutex_unlock(&e->mtx);

    /* 从表中移除 */
    pthread_mutex_lock(&g_inflight_mutex);
    inflight_free_locked(e);
    pthread_mutex_unlock(&g_inflight_mutex);

    return result;
}

void cmd_mgr_inflight_signal(int fd, uint16_t packet_id) {
    pthread_mutex_lock(&g_inflight_mutex);
    for (int i = 0; i < MAX_INFLIGHT; i++) {
        inflight_entry_t *e = &g_inflight[i];
        if (e->used && e->fd == fd && e->packet_id == packet_id) {
            pthread_mutex_lock(&e->mtx);
            e->acked = 1;
            pthread_cond_signal(&e->cond);
            pthread_mutex_unlock(&e->mtx);
            pthread_mutex_unlock(&g_inflight_mutex);
            LOG_INFO("inflight signal fd=%d pid=%u cmd=%s", fd, packet_id, e->cmd_id);
            return;
        }
    }
    pthread_mutex_unlock(&g_inflight_mutex);
    /* 没有匹配 inflight 条目也 OK — 可能是 replayed 命令 */
}

/* ----------------------------------------------------------------- */
/* 离线队列 (STAILQ)                                                   */
/* ----------------------------------------------------------------- */
void cmd_mgr_enqueue_offline(const char *client_id,
                             const char *cmd_name,
                             const char *cmd_payload,
                             const char *command_id) {
    offline_cmd_t *node = calloc(1, sizeof(*node));
    if (!node) {
        LOG_ERROR("OOM enqueue offline");
        return;
    }
    strncpy(node->client_id, client_id, sizeof(node->client_id) - 1);
    strncpy(node->cmd_name, cmd_name, sizeof(node->cmd_name) - 1);
    strncpy(node->cmd_payload, cmd_payload, sizeof(node->cmd_payload) - 1);
    strncpy(node->command_id, command_id, sizeof(node->command_id) - 1);
    node->queued_at = time(NULL);

    pthread_mutex_lock(&g_offline_mutex);
    STAILQ_INSERT_HEAD(&g_offline_queue, node, entry);
    pthread_mutex_unlock(&g_offline_mutex);
    LOG_INFO("offline enqueued client=%s cmd=%s id=%s", client_id, cmd_name, command_id);
}

offline_cmd_t *cmd_mgr_dequeue_all(const char *client_id) {
    offline_cmd_t *result = NULL;
    offline_cmd_t *cur, *tmp;

    pthread_mutex_lock(&g_offline_mutex);
    STAILQ_FOREACH_SAFE(cur, &g_offline_queue, entry, tmp) {
        if (strcmp(cur->client_id, client_id) == 0) {
            STAILQ_REMOVE(&g_offline_queue, cur, offline_cmd, entry);
            cur->entry.stqe_next = result;
            result = cur;
        }
    }
    pthread_mutex_unlock(&g_offline_mutex);
    return result;
}

void cmd_mgr_free_offline_list(offline_cmd_t *list) {
    offline_cmd_t *cur;
    while (!STAILQ_EMPTY((struct offline_cmd_queue *)&list)) {
        cur = STAILQ_FIRST((struct offline_cmd_queue *)&list);
        STAILQ_REMOVE_HEAD((struct offline_cmd_queue *)&list, entry);
        free(cur);
    }
}
