/**
 * @file shard_router.c
 *
 * P5 分表路由 — 按月分表
 */
#include "data/shard_router.h"
#include "common/log.h"

#include <stdio.h>
#include <string.h>

int shard_router_table_by_time(time_t ts, char *out, size_t cap) {
    struct tm tm;
    gmtime_r(&ts, &tm);
    snprintf(out, cap, "data_%04d%02d", tm.tm_year + 1900, tm.tm_mon + 1);
    return 0;
}

int shard_router_table(const char *device_id, time_t ts, char *out, size_t cap) {
    (void)device_id;
    return shard_router_table_by_time(ts, out, cap);
}

uint32_t shard_router_hash(const char *device_id) {
    /* FNV-1a */
    uint32_t h = 2166136261u;
    for (const unsigned char *p = (const unsigned char *)device_id; *p; p++) {
        h ^= *p;
        h *= 16777619u;
    }
    return h;
}

int shard_router_ensure_table(const char *table_name) {
    /* 真实实现: CREATE TABLE IF NOT EXISTS table_name ... */
    LOG_DEBUG("shard ensure table: %s", table_name);
    return 0;
}
