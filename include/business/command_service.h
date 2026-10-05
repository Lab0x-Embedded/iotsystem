/**
 * @file command_service.h
 *
 * P5 指令下行 — 离线队列 + inflight 跟踪 + 线程同步
 *
 * 设计:
 *   - 离线命令 STAILQ 队列, g_offline_mutex 保护
 *   - Inflight 表跟踪已下发等待 PUBACK 的命令
 *   - HTTP 线程等待 COND, event loop 收到 PUBACK 后 signal
 */
#ifndef E2_COMMAND_SERVICE_H
#define E2_COMMAND_SERVICE_H

#include <stdint.h>
#include <sys/queue.h>
#include <time.h>
#include <pthread.h>

#define CMD_ID_LEN    32
#define CMD_BODY_LEN  512
#define MAX_INFLIGHT  64

#ifdef __cplusplus
extern "C" {
#endif

/* ----------------------------------------------------------------- */
/* Inflight 条目                                                      */
/* ----------------------------------------------------------------- */
typedef struct inflight_entry {
    int      used;
    int      fd;
    uint16_t packet_id;
    volatile int acked;
    time_t   sent_at;
    char     cmd_id[CMD_ID_LEN];
    pthread_cond_t  cond;
    pthread_mutex_t mtx;
} inflight_entry_t;

/* ----------------------------------------------------------------- */
/* 离线命令节点 (STAILQ)                                               */
/* ----------------------------------------------------------------- */
typedef struct offline_cmd {
    char    client_id[129];   /* 与 MQTT_ID_MAX 对齐(该头文件不引 mqtt_types.h, 故写字面量) */
    char    cmd_name[64];
    char    cmd_payload[CMD_BODY_LEN];
    char    command_id[CMD_ID_LEN];
    time_t  queued_at;
    STAILQ_ENTRY(offline_cmd) entry;
} offline_cmd_t;

STAILQ_HEAD(offline_cmd_queue, offline_cmd);

/* ----------------------------------------------------------------- */
/* API                                                               */
/* ----------------------------------------------------------------- */

/** 初始化 (pthread_once 调用). */
void cmd_mgr_init(void);

/** 分配一个 inflight 条目并等待 ack. 返回 1=acked, 0=timeout. */
int cmd_mgr_inflight_wait(int fd, uint16_t packet_id, const char *cmd_id, int timeout_sec);

/** Event loop 收到 PUBACK 后调用: 找到 inflight 条目并 signal. */
void cmd_mgr_inflight_signal(int fd, uint16_t packet_id);

/** 离线入队. */
void cmd_mgr_enqueue_offline(const char *client_id,
                             const char *cmd_name,
                             const char *cmd_payload,
                             const char *command_id);

/** 出队某设备的所有离线命令; 调用者 free. */
offline_cmd_t *cmd_mgr_dequeue_all(const char *client_id);

/** 释放 offline_cmd_t 链表. */
void cmd_mgr_free_offline_list(offline_cmd_t *list);

#ifdef __cplusplus
}
#endif

#endif /* E2_COMMAND_SERVICE_H */
