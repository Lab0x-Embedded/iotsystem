/**
 * @file shard_router.h
 *
 * P5 分表路由 — 按月分表 + device_id 哈希
 *
 * 设计:
 *   - 表名: data_YYYYMM (如 data_202607)
 *   - 路由: shard_router_table(device_id, timestamp) -> 表名
 *   - 支持自动建表 (存储过程)
 */
#ifndef E2_SHARD_ROUTER_H
#define E2_SHARD_ROUTER_H

#include <stdint.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SHARD_TABLE_LEN 64

/** 根据时间戳计算分表表名. */
int shard_router_table_by_time(time_t ts, char *out, size_t cap);

/** 根据 device_id + 时间戳计算分表表名. */
int shard_router_table(const char *device_id, time_t ts, char *out, size_t cap);

/** 计算 device_id 哈希 (用于多库分片). */
uint32_t shard_router_hash(const char *device_id);

/** 确保分表存在 (不存在则创建). */
int shard_router_ensure_table(const char *table_name);

#ifdef __cplusplus
}
#endif

#endif /* E2_SHARD_ROUTER_H */
