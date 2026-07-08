/**
 * @file query_service.h
 *
 * P5 查询优化 — 复合索引 + 时间范围查询
 *
 * 设计:
 *   - 主查询: device_id + 时间范围 → 历史数据
 *   - 利用 (device_id, ts) 复合索引
 *   - 分页 + 降采样支持
 */
#ifndef E2_QUERY_SERVICE_H
#define E2_QUERY_SERVICE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define QUERY_POINTS_MAX 1024

typedef struct {
    uint64_t ts;
    double   value;
} query_point_t;

typedef struct {
    char    device_id[65];
    char    metric[64];
    uint64_t start_ts;
    uint64_t end_ts;
    int      limit;
} query_request_t;

/**
 * 查询设备历史数据.
 *  @param req     查询请求
 *  @param out     结果缓冲区
 *  @param max_n   缓冲区容量
 *  @return 实际返回点数, -1 出错
 */
int query_history(const query_request_t *req, query_point_t *out, int max_n);

/** 最新值查询. */
int query_latest(const char *device_id, const char *metric, double *out_value,
                 uint64_t *out_ts);

/** 聚合查询 (MIN/AVG/MAX). */
int query_aggregate(const char *device_id, const char *metric,
                    uint64_t start_ts, uint64_t end_ts,
                    double *out_min, double *out_avg, double *out_max);

#ifdef __cplusplus
}
#endif

#endif /* E2_QUERY_SERVICE_H */
