/**
 * @file shadow_manager.h
 *
 * P4 设备影子 (desired/reported/delta) — 三段式架构
 *
 * 设计:
 *   - desired:  期望状态 (应用层写入)
 *   - reported: 实际上报状态
 *   - delta:    desired - reported 差异 (自动生成)
 *   * 设备在线时, delta != 0 → 下发校正指令
 */
#ifndef E2_SHADOW_MANAGER_H
#define E2_SHADOW_MANAGER_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SHADOW_KEY_LEN   64
#define SHADOW_VAL_LEN   256
#define SHADOW_MAX_KVS   16
#define SHADOW_DEV_LEN   65

typedef struct {
    char key[SHADOW_KEY_LEN];
    char value[SHADOW_VAL_LEN];
    uint64_t version;
} shadow_kv_t;

typedef struct {
    char       device_id[SHADOW_DEV_LEN];
    shadow_kv_t desired[SHADOW_MAX_KVS];
    shadow_kv_t reported[SHADOW_MAX_KVS];
    int        desired_n;
    int        reported_n;
    uint64_t   version;
} device_shadow_t;

int  shadow_manager_init(void);

/** 设置 desired. */
int  shadow_set_desired(const char *device_id, const char *key, const char *value);

/** 更新 reported (设备上报时调用). */
int  shadow_update_reported(const char *device_id, const char *key, const char *value);

/** 计算 delta: 返回/desired-reported/的键值对 (写入 out_delta, 返回数量). */
int  shadow_compute_delta(const char *device_id,
                          shadow_kv_t *out_delta, int max_n);

/** 校验 device shadow 是否存在. */
const device_shadow_t *shadow_find(const char *device_id);

#ifdef __cplusplus
}
#endif

#endif /* E2_SHADOW_MANAGER_H */
