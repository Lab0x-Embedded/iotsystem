/**
 * @file query_service.c
 *
 * P5 查询优化 — 真实实现
 */
#include "data/query_service.h"
#include "data/shard_router.h"
#include "data/db_pool.h"
#include "common/log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mysql.h>

int query_history(const query_request_t *req, query_point_t *out, int max_n) {
    if (!req || !out || max_n <= 0) return -1;

    db_conn_t *conn = db_pool_get();
    if (!conn) {
        LOG_ERROR("query_history: no db connection");
        return -1;
    }

    // 构建查询SQL
    char sql[512];
    char table_name[64];
    shard_router_table(req->device_id, (time_t)req->start_ts, table_name, sizeof(table_name));
    
    snprintf(sql, sizeof(sql),
        "SELECT ts, value FROM %s "
        "WHERE device_id = '%s' AND metric = '%s' "
        "AND ts >= %llu AND ts <= %llu "
        "ORDER BY ts DESC LIMIT %d",
        table_name,
        req->device_id,
        req->metric,
        (unsigned long long)req->start_ts,
        (unsigned long long)req->end_ts,
        max_n);

    MYSQL_RES *result = db_pool_query(conn, sql);
    if (!result) {
        db_pool_put(conn);
        return -1;
    }

    int count = 0;
    MYSQL_ROW row;
    while ((row = mysql_fetch_row(result)) && count < max_n) {
        out[count].ts = strtoull(row[0], NULL, 10);
        out[count].value = strtod(row[1], NULL);
        count++;
    }

    db_pool_free_result(result);
    db_pool_put(conn);
    
    LOG_DEBUG("query_history: returned %d points for %s", count, req->device_id);
    return count;
}

int query_latest(const char *device_id, const char *metric, double *out_value,
                 uint64_t *out_ts) {
    if (!device_id || !metric) return -1;

    db_conn_t *conn = db_pool_get();
    if (!conn) {
        LOG_ERROR("query_latest: no db connection");
        return -1;
    }

    // 查询最新值（从device_latest_data表）
    char sql[256];
    snprintf(sql, sizeof(sql),
        "SELECT value, ts FROM device_latest_data "
        "WHERE device_id = '%s' AND metric = '%s' "
        "ORDER BY ts DESC LIMIT 1",
        device_id, metric);

    MYSQL_RES *result = db_pool_query(conn, sql);
    if (!result) {
        db_pool_put(conn);
        return -1;
    }

    MYSQL_ROW row = mysql_fetch_row(result);
    if (row) {
        if (out_value) *out_value = strtod(row[0], NULL);
        if (out_ts) *out_ts = strtoull(row[1], NULL, 10);
        db_pool_free_result(result);
        db_pool_put(conn);
        return 0;
    }

    db_pool_free_result(result);
    db_pool_put(conn);
    return -1;
}

int query_aggregate(const char *device_id, const char *metric,
                    uint64_t start_ts, uint64_t end_ts,
                    double *out_min, double *out_avg, double *out_max) {
    if (!device_id || !metric) return -1;

    db_conn_t *conn = db_pool_get();
    if (!conn) {
        LOG_ERROR("query_aggregate: no db connection");
        return -1;
    }

    // 构建聚合查询
    char sql[512];
    char table_name[64];
    shard_router_table(device_id, (time_t)start_ts, table_name, sizeof(table_name));
    
    snprintf(sql, sizeof(sql),
        "SELECT MIN(value), AVG(value), MAX(value) FROM %s "
        "WHERE device_id = '%s' AND metric = '%s' "
        "AND ts >= %llu AND ts <= %llu",
        table_name,
        device_id,
        metric,
        (unsigned long long)start_ts,
        (unsigned long long)end_ts);

    MYSQL_RES *result = db_pool_query(conn, sql);
    if (!result) {
        db_pool_put(conn);
        return -1;
    }

    MYSQL_ROW row = mysql_fetch_row(result);
    if (row) {
        if (out_min) *out_min = strtod(row[0], NULL);
        if (out_avg) *out_avg = strtod(row[1], NULL);
        if (out_max) *out_max = strtod(row[2], NULL);
        db_pool_free_result(result);
        db_pool_put(conn);
        return 0;
    }

    db_pool_free_result(result);
    db_pool_put(conn);
    return -1;
}
