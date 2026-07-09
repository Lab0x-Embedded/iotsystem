/**
 * @file data_writer.c
 *
 * P5 批量异步写入 — 真实实现
 */
#include "data/data_writer.h"
#include "data/shard_router.h"
#include "data/db_pool.h"
#include "common/log.h"

#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <time.h>

#define BATCH_SIZE 64
#define MAX_SQL_LEN 4096

typedef struct {
    char     device_id[65];
    char     metric[64];
    double   value;
    uint64_t ts;
} data_point_t;

static data_point_t g_buf[BATCH_SIZE];
static int          g_head = 0;
static int          g_count = 0;
static pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;

/** 批量写入数据库 */
static int flush_to_db(const data_point_t *points, int count) {
    if (count <= 0) return 0;

    db_conn_t *conn = db_pool_get();
    if (!conn) {
        LOG_ERROR("data_writer: no db connection");
        return -1;
    }

    // 构建批量INSERT语句
    char sql[MAX_SQL_LEN];
    int offset = 0;
    
    // 获取当前分表名
    time_t now = time(NULL);
    char table_name[64];
    shard_router_table(points[0].device_id, now, table_name, sizeof(table_name));
    
    // 开始事务
    db_pool_exec(conn, "START TRANSACTION");
    
    offset = snprintf(sql, sizeof(sql),
        "INSERT INTO %s (device_id, metric, value, ts) VALUES ",
        table_name);

    for (int i = 0; i < count; i++) {
        if ((size_t)offset >= sizeof(sql) - 200) {
            // SQL太长，先执行当前批次
            sql[offset - 1] = '\0';  // 去掉最后的逗号
            if (db_pool_exec(conn, sql) != 0) {
                LOG_ERROR("data_writer: batch insert failed");
                db_pool_exec(conn, "ROLLBACK");
                db_pool_put(conn);
                return -1;
            }
            offset = snprintf(sql, sizeof(sql),
                "INSERT INTO %s (device_id, metric, value, ts) VALUES ",
                table_name);
        }
        
        offset += snprintf(sql + offset, sizeof(sql) - offset,
            "('%s', '%s', %f, %llu)%s",
            points[i].device_id,
            points[i].metric,
            points[i].value,
            (unsigned long long)points[i].ts,
            (i < count - 1) ? "," : "");
    }

    // 执行最后一批
    if (offset > 0) {
        sql[offset] = '\0';
        if (db_pool_exec(conn, sql) != 0) {
            LOG_ERROR("data_writer: final batch insert failed");
            db_pool_exec(conn, "ROLLBACK");
            db_pool_put(conn);
            return -1;
        }
    }

    // 提交事务
    db_pool_exec(conn, "COMMIT");
    db_pool_put(conn);
    
    LOG_DEBUG("data_writer: wrote %d points to %s", count, table_name);
    return 0;
}

int data_writer_init(void) {
    pthread_mutex_lock(&g_lock);
    memset(g_buf, 0, sizeof(g_buf));
    g_head = 0;
    g_count = 0;
    pthread_mutex_unlock(&g_lock);
    LOG_INFO("data_writer initialized (batch=%d)", BATCH_SIZE);
    return 0;
}

void data_writer_shutdown(void) {
    data_writer_flush();
    LOG_INFO("data_writer shutdown");
}

int data_writer_enqueue(const char *device_id, const char *metric,
                        double value, uint64_t ts) {
    if (!device_id || !metric) return -1;

    pthread_mutex_lock(&g_lock);
    if (g_count >= BATCH_SIZE) {
        pthread_mutex_unlock(&g_lock);
        data_writer_flush();
        pthread_mutex_lock(&g_lock);
    }
    
    data_point_t *d = &g_buf[g_count++];
    strncpy(d->device_id, device_id, 64);
    strncpy(d->metric, metric, 63);
    d->value = value;
    d->ts = ts;
    pthread_mutex_unlock(&g_lock);
    
    return 0;
}

void data_writer_flush(void) {
    pthread_mutex_lock(&g_lock);
    int n = g_count;
    g_count = 0;
    g_head = 0;
    pthread_mutex_unlock(&g_lock);
    
    if (n > 0) {
        if (flush_to_db(g_buf, n) != 0) {
            LOG_ERROR("data_writer: flush failed, %d points lost", n);
        }
    }
}

int data_writer_pending(void) {
    int n;
    pthread_mutex_lock(&g_lock);
    n = g_count;
    pthread_mutex_unlock(&g_lock);
    return n;
}
