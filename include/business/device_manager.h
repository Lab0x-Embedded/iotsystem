/**
 * @file device_manager.h
 *
 * P4 设备注册/认证/状态 — 设备全生命周期管理 (MySQL 持久化, 无内存缓存)
 *
 * 设计:
 *   - 生命周期态 (lifecycle state): REGISTERED -> ACTIVE -> DISABLED
 *   - 瞬时在线态 (online flag): 独立于 lifecycle state，由 presence 标记 + DB `online` 列维护
 *   - 表结构对齐: 见 deploy/sql/init_schema.sql 的 `devices` 表
 *   - presence 标记: device_manager_presence() 只入队不落库, 由后台线程每 5s 批量下刷;
 *     超过 90s 无活动的在线设备由巡检 SQL 批量判离线 (online=0, state 不变)
 *   - 线程安全: device_manager_presence()/online()/offline() 可从任意线程调用 (非阻塞)
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

/** 初始化设备管理器 (启动 presence 下刷线程; DB 不可用也能初始化). */
int  device_manager_init(void);

/** 停机 (停 presence 线程并落库剩余标记; 必须在 db_pool_shutdown 之前调用). */
void device_manager_shutdown(void);

/**
 * 注册新设备.
 *  @param device_id      设备 id (长度须 < DEV_ID_LEN, 超长返回 -1)
 *  @param name           显示名 (可为 NULL)
 *  @param product_key    产品 key
 *  @param device_type    设备类型 (可为 NULL, 默认 "")
 *  @param device_secret  认证密钥 (可为 NULL)
 *  @param group_id       分组 id
 *  @return 0 成功, -1 失败
 */
int  device_register(const char *device_id, const char *name,
                     const char *product_key, const char *device_type,
                     const char *device_secret, int group_id);

/**
 * 设备激活 (上线): 可选校验 device_secret, 命中则置 status='active' + online=TRUE.
 *  @param device_secret  为 NULL 时只按 device_id 匹配 (兼容旧接口)
 *  @return 0 激活成功, -1 设备不存在或 secret 不匹配, -2 DB 异常
 */
int  device_manager_activate(const char *device_id, const char *device_secret);

/** 设备上线 (非阻塞: 标记入队, 由 presence 线程下刷). */
void device_manager_online(const char *device_id);

/** 设备心跳 (同步刷新 last_online). */
void device_manager_heartbeat(const char *device_id);

/** 设备下线 (非阻塞: 标记入队, 由 presence 线程下刷). */
void device_manager_offline(const char *device_id);

/**
 * presence 标记 (非阻塞, 任意线程可调).
 *  @param device_id  设备 id
 *  @param online     true=在线(同时刷新 last_online), false=离线
 */
void device_manager_presence(const char *device_id, bool online);

/**
 * 查询设备信息.
 *  @param device_id  设备 id
 *  @param out_info   [out] 调用方提供的缓冲区
 *  @return 0 找到, -1 不存在或参数错误
 */
int  device_manager_find(const char *device_id, device_info_t *out_info);

/** 周期性巡检: 将超时设备标记为 online=false (state 不变). */
void device_manager_tick(time_t now);

/**
 * 单条 SQL 批量巡检: online=1 且 last_online 早于 timeout_sec 的设备置为离线.
 *  @param timeout_sec  <=0 时使用默认值 (90s)
 *  @return 被置为离线的设备数, -1 失败
 */
int  device_manager_sweep(int timeout_sec);

/** 获取在线设备数量. */
int  device_manager_online_count(void);

/** 移除设备 (状态置为 DEV_STATE_DISABLED). */
int  device_manager_decommission(const char *device_id);

/** 获取所有设备列表 (用于 API 查询).
 *  @return 动态分配数组, 调用者必须 free() */
int device_manager_get_all(const device_info_t **devices, int *count);

/** 获取设备总数. */
int device_manager_total_count(void);

/**
 * 按 group_id 获取设备列表.
 * @param group_id  目标分组 id
 * @param devices   [out] 设备数组指针 (动态分配, 调用者必须 free())
 * @param count     [out] 匹配的设备数量
 * @return 0 成功, -1 参数错误
 */
int device_manager_get_by_group(int group_id, const device_info_t **devices, int *count);

/** 更新设备分组. */
int device_manager_update_group(const char *device_id, int group_id);

/** 更新设备名称. */
int device_manager_update_name(const char *device_id, const char *name);

#ifdef __cplusplus
}
#endif

#endif /* E2_DEVICE_MANAGER_H */
