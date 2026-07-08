/**
 * @file device_manager.h
 *
 * P4 设备注册/认证/状态 — 设备全生命周期管理
 *
 * 设计:
 *   - 设备表 (内存索引 + MySQL 持久化)
 *   - 状态机: REGISTERED -> ACTIVE -> ONLINE <-> OFFLINE
 *   - 心跳保活: 超过 keepalive*1.5 无活动判定离线
 */
#ifndef E2_DEVICE_MANAGER_H
#define E2_DEVICE_MANAGER_H

#include <stdint.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    DEV_STATE_REGISTERED = 0,
    DEV_STATE_ACTIVE,
    DEV_STATE_ONLINE,
    DEV_STATE_OFFLINE,
    DEV_STATE_DISABLED,
} device_state_t;

#define DEV_ID_LEN   65
#define DEV_NAME_LEN 128

typedef struct {
    char     device_id[DEV_ID_LEN];
    char     name[DEV_NAME_LEN];
    char     product_key[33];
    char     group_id[DEV_ID_LEN];
    device_state_t state;
    uint16_t keepalive;
    time_t   last_active;
    time_t   registered_at;
    uint32_t report_count;
} device_info_t;

/** 初始化设备管理器 (加载全表到内存索引). */
int  device_manager_init(void);

/** 注册新设备; 返回 0 成功, -1 失败. */
int  device_register(const char *device_id, const char *name,
                     const char *product_key, const char *group_id);

/** 设备上线/心跳. */
void device_manager_online(const char *device_id, uint16_t keepalive);
void device_manager_heartbeat(const char *device_id);

/** 设备下线. */
void device_manager_offline(const char *device_id);

/** 查询设备信息 (NULL=不存在). */
const device_info_t *device_manager_find(const char *device_id);

/** 周期性巡检: 将超时设备标记为 OFFLINE. */
void device_manager_tick(time_t now);

/** 获取在线设备数量. */
int  device_manager_online_count(void);

#ifdef __cplusplus
}
#endif

#endif /* E2_DEVICE_MANAGER_H */
