/**
 * @file query_service.c
 *
 * P5 查询优化 — 桩实现
 */
#include "data/query_service.h"
#include "data/shard_router.h"
#include "data/db_pool.h"
#include "common/log.h"

int query_history(const query_request_t *req, query_point_t *out, int max_n) {
    if (!req || !out || max_n <= 0) return -1;
    (void)req; (void)out; (void)max_n;
    return 0;
}

int query_latest(const char *device_id, const char *metric, double *out_value,
                 uint64_t *out_ts) {
    if (!device_id || !metric) return -1;
    (void)out_value; (void)out_ts;
    return -1;
}

int query_aggregate(const char *device_id, const char *metric,
                    uint64_t start_ts, uint64_t end_ts,
                    double *out_min, double *out_avg, double *out_max) {
    if (!device_id || !metric) return -1;
    (void)start_ts; (void)end_ts;
    (void)out_min; (void)out_avg; (void)out_max;
    return -1;
}
