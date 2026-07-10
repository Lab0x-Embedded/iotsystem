/**
 * @file device_manager.h
 *
 * P4 设备注册/认证/状态 — 设备全生命周期管理 (内存索引 + MySQL 持久化)
 *
 * 设计:
 *   - 生命周期态 (lifecycle state): REGISTERED -> ACTIVE -> DISABLED
 *   - 瞬时在线态 (online flag): 独立于 lifecycle state，由心跳/离线事件和 DB `online` 列维护
 *   - 表结构对齐: 见 deploy/sql/init.sql 的 `devices` 表
 *   - 心跳保活: 超过 keepalive*1.5 无活动判定离线 (online=false, state 不变)
 */
#ifndef E2_DEVICE_MANAGER_H
#define E2_DEVICE_MANAGER_H

#include <stdint.h>
#include <time.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    DEV_STATE_REGISTERED = 0,
    DEV_STATE_ACTIVE,        /* 1 — 已激活 */
    DEV_STATE_BANNED,        /* 2 — 已封禁 (预留) */
    DEV_STATE_MAINTENANCE,   /* 3 — 维护中 (预留, UI 显示用) */
    DEV_STATE_DISABLED,      /* 4 — 已禁用 */
} device_state_t;

#define DEV_ID_LEN     65   /* 对齐 devices.device_id VARCHAR(64) + 1 */
#define DEV_NAME_LEN   128
#define DEV_TYPE_LEN   64   /* 对齐 devices.device_type VARCHAR(64) */
#define DEV_SECRET_LEN 255  /* 对齐 devices.device_secret VARCHAR(255) */
#define DEV_PK_LEN     65   /* 对齐 devices.product_key VARCHAR(64) + 1 */

typedef struct {
    char           device_id[DEV_ID_LEN];
    char           name[DEV_NAME_LEN];
    char           product_key[DEV_PK_LEN];
    char           device_type[DEV_TYPE_LEN];
    char           device_secret[DEV_SECRET_LEN];
    int            group_id;
    device_state_t state;
    bool           online;          /* 瞬时在线态, 与 DB `online` 列一致 */
    uint16_t       keepalive;       /* 秒; 仅内存用, 不持久化 */
    time_t         last_active;     /* 最近心跳时间 (内存, 对应 DB last_online) */
    time_t         registered_at;
    time_t         last_online;     /* 最近一次 DB 持久化的在线时间 */
    time_t         updated_at;      /* 与 DB updated_at 列一致 */
    uint32_t       report_count;    /* 仅内存用, 不持久化 */
} device_info_t;

/** 初始化设备管理器 (加载全表到内存索引). */
int  device_manager_init(void);

/**
 * 注册新设备.
 *  @param device_id      设备 id
 *  @param name           显示名 (可为 NULL)
 *  @param product_key    产品 key
 *  @param device_type    设备类型 (可为 NULL, 默认 "")
 *  @param device_secret  认证密钥 (可为 NULL, 服务将自动生成)
 *  @param group_id       分组 id
 *  @return 0 成功, -1 失败
 */
int  device_register(const char *device_id, const char *name,
                     const char *product_key, const char *device_type,
                     const char *device_secret, int group_id);

/** 设备上线/心跳. */
void device_manager_online(const char *device_id, uint16_t keepalive);
void device_manager_heartbeat(const char *device_id);

/** 设备下线. */
void device_manager_offline(const char *device_id);

/** 查询设备信息 (NULL=不存在). */
const device_info_t *device_manager_find(const char *device_id);

/** 周期性巡检: 将超时设备标记为 online=false (state 不变). */
void device_manager_tick(time_t now);

/** 获取在线设备数量. */
int  device_manager_online_count(void);

/** 移除设备 (状态置为 DEV_STATE_DISABLED). */
int  device_manager_decommission(const char *device_id);

#ifdef __cplusplus
}
#endif

#endif /* E2_DEVICE_MANAGER_H */

/** 获取所有设备列表 (用于 API 查询). */
int device_manager_get_all(const device_info_t **devices, int *count);

/** 获取设备总数. */
int device_manager_total_count(void);
