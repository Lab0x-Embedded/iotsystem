/**
 * @file data_writer.c
 *
 * P5 批量异步写入
 *
 * 架构：
 *
 *   producer
 *      |
 *      v
 *   active batch
 *      |
 *      | 64条 / 1秒
 *      v
 *   batch queue
 *      |
 *      v
 *   worker thread
 *      |
 *      v
 *   db_pool
 *      |
 *      v
 *   MySQL
 *
 * 特性：
 * 1. pthread worker
 * 2. pthread condition variable
 * 3. 64 条批量写入
 * 4. 1 秒超时自动 flush
 * 5. 有界 batch queue
 * 6. DB 操作不阻塞普通 enqueue
 * 7. queue 满时采用背压，不丢数据
 * 8. shutdown 等待剩余数据处理
 */

#include "data/data_writer.h"
#include "data/shard_router.h"
#include "data/db_pool.h"
#include "common/log.h"

#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>

#define BATCH_SIZE 64
#define BATCH_QUEUE_SIZE 8
#define MAX_SQL_LEN 4096
#define FLUSH_INTERVAL_MS 1000

typedef struct {
    char device_id[65];
    char metric[64];
    double value;
    uint64_t ts;
} data_point_t;

/**
 * 一个 batch。
 */
typedef struct {
    data_point_t points[BATCH_SIZE];
    int count;
} data_batch_t;

/**
 * 当前正在接收数据的 batch。
 */
static data_point_t g_active_buf[BATCH_SIZE];
static int g_active_count = 0;

/**
 * 等待 worker 消费的 batch 队列。
 *
 * queue:
 *
 *   [0] [1] [2] [3] ... [7]
 *    ^
 *    tail
 *
 *          ^
 *          head
 */
static data_batch_t g_queue[BATCH_QUEUE_SIZE];

static int g_queue_head = 0;
static int g_queue_tail = 0;
static int g_queue_count = 0;

/**
 * worker。
 */
static pthread_t g_worker;

/**
 * 保护整个 writer 状态。
 */
static pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;

/**
 * 有数据：
 *
 * producer -> worker
 */
static pthread_cond_t g_not_empty = PTHREAD_COND_INITIALIZER;

/**
 * queue 有空间：
 *
 * worker -> producer
 */
static pthread_cond_t g_not_full = PTHREAD_COND_INITIALIZER;

/**
 * writer 状态。
 */
static int g_initialized = 0;
static int g_running = 0;
static int g_shutdown_requested = 0;

/**
 * 计算 timeout。
 */
static void make_timeout(struct timespec *ts, long timeout_ms) {
    clock_gettime(CLOCK_REALTIME, ts);

    ts->tv_sec += timeout_ms / 1000;

    ts->tv_nsec += (timeout_ms % 1000) * 1000000L;

    if (ts->tv_nsec >= 1000000000L) {
        ts->tv_sec++;
        ts->tv_nsec -= 1000000000L;
    }
}

/**
 * 将当前 active batch 放入 queue。
 *
 * 注意：
 *
 * 调用者必须持有 g_lock。
 *
 * 返回：
 *   0  成功
 *  -1  queue 满
 */
static int queue_active_batch_locked(void) {
    if (g_active_count <= 0) {
        return 0;
    }

    if (g_queue_count >= BATCH_QUEUE_SIZE) {
        return -1;
    }

    data_batch_t *batch = &g_queue[g_queue_tail];

    memcpy(batch->points, g_active_buf, sizeof(data_point_t) * g_active_count);

    batch->count = g_active_count;

    g_queue_tail++;

    if (g_queue_tail >= BATCH_QUEUE_SIZE) {
        g_queue_tail = 0;
    }

    g_queue_count++;

    g_active_count = 0;

    /*
     * 通知 worker：
     *
     * queue 有数据了。
     */
    pthread_cond_signal(&g_not_empty);

    return 0;
}

/**
 * 构建 INSERT SQL。
 *
 * 注意：
 *
 * 当前仍然是字符串拼接。
 *
 * 如果 device_id / metric 来自不可信输入，
 * 后续建议改 prepared statement。
 */
static int build_insert_sql(const data_point_t *points, int count, char *sql, size_t sql_size,
                            const char *table_name) {
    if (!points || count <= 0 || !sql || sql_size == 0 || !table_name) {

        return -1;
    }

    int offset = snprintf(sql, sql_size,

                          "INSERT INTO %s "
                          "(device_id, metric, value, ts) VALUES ",

                          table_name);

    if (offset < 0 || (size_t)offset >= sql_size) {

        return -1;
    }

    for (int i = 0; i < count; ++i) {

        int written = snprintf(sql + offset, sql_size - (size_t)offset,

                               "('%s', '%s', %.17g, %llu)%s",

                               points[i].device_id, points[i].metric, points[i].value,

                               (unsigned long long)points[i].ts,

                               i == count - 1 ? "" : ",");

        if (written < 0) {
            return -1;
        }

        if ((size_t)written >= sql_size - (size_t)offset) {

            return -1;
        }

        offset += written;
    }

    return 0;
}

/**
 * 将一个 batch 写入数据库。
 *
 * 一个 batch 必须属于同一张表。
 */
static int flush_batch_to_db(const data_point_t *points, int count) {
    if (!points || count <= 0) {
        return 0;
    }

    db_conn_t *conn = db_pool_get();

    if (!conn) {

        LOG_ERROR("data_writer: "
                  "no db connection");

        return -1;
    }

    char table_name[64];

    shard_router_table(points[0].device_id, (time_t)points[0].ts, table_name, sizeof(table_name));

    char sql[MAX_SQL_LEN];

    /*
     * 开启事务。
     */
    if (db_pool_exec(conn, "START TRANSACTION") != 0) {

        LOG_ERROR("data_writer: "
                  "START TRANSACTION failed");

        db_pool_put(conn);

        return -1;
    }

    /*
     * 构建 SQL。
     */
    if (build_insert_sql(points, count, sql, sizeof(sql), table_name) != 0) {

        LOG_ERROR("data_writer: "
                  "SQL too large, "
                  "table=%s count=%d",
                  table_name, count);

        db_pool_exec(conn, "ROLLBACK");

        db_pool_put(conn);

        return -1;
    }

    /*
     * 批量 INSERT。
     */
    if (db_pool_exec(conn, sql) != 0) {

        LOG_ERROR("data_writer: "
                  "INSERT failed, "
                  "table=%s count=%d",
                  table_name, count);

        db_pool_exec(conn, "ROLLBACK");

        db_pool_put(conn);

        return -1;
    }

    /*
     * 提交。
     */
    if (db_pool_exec(conn, "COMMIT") != 0) {

        LOG_ERROR("data_writer: "
                  "COMMIT failed, "
                  "table=%s count=%d",
                  table_name, count);

        db_pool_exec(conn, "ROLLBACK");

        db_pool_put(conn);

        return -1;
    }

    db_pool_put(conn);

    LOG_DEBUG("data_writer: "
              "wrote %d points to %s",
              count, table_name);

    return 0;
}

/**
 * 写入一个 batch。
 *
 * 这里处理：
 *
 * batch 内可能跨不同分表的问题。
 */
static int flush_to_db(const data_point_t *points, int count) {
    if (!points || count <= 0) {
        return 0;
    }

    int used[BATCH_SIZE];

    memset(used, 0, sizeof(used));

    int total = 0;

    for (int i = 0; i < count; ++i) {

        if (used[i]) {
            continue;
        }

        /*
         * 获取当前数据对应的表。
         */
        char table_name[64];

        shard_router_table(points[i].device_id, (time_t)points[i].ts, table_name,
                           sizeof(table_name));

        /*
         * 收集同一张表的数据。
         */
        data_point_t group[BATCH_SIZE];

        int group_count = 0;

        for (int j = i; j < count; ++j) {

            if (used[j]) {
                continue;
            }

            char current_table[64];

            shard_router_table(points[j].device_id, (time_t)points[j].ts, current_table,
                               sizeof(current_table));

            if (strcmp(table_name, current_table) == 0) {

                group[group_count++] = points[j];

                used[j] = 1;
            }
        }

        /*
         * 写这一张表。
         */
        if (group_count > 0) {

            if (flush_batch_to_db(group, group_count) != 0) {

                LOG_ERROR("data_writer: "
                          "batch flush failed, "
                          "table=%s count=%d",
                          table_name, group_count);

                return -1;
            }

            total += group_count;
        }
    }

    return total == count ? 0 : -1;
}

/**
 * worker。
 */
static void *data_writer_worker(void *arg) {
    (void)arg;

    LOG_INFO("data_writer: worker started");

    while (1) {

        data_batch_t batch;

        memset(&batch, 0, sizeof(batch));

        pthread_mutex_lock(&g_lock);

        /*
         * 等待条件：
         *
         * 1. queue 有 batch
         *
         * 或
         *
         * 2. active buffer 超时
         *
         * 或
         *
         * 3. shutdown
         */
        while (g_queue_count == 0 && g_active_count == 0 && !g_shutdown_requested) {

            struct timespec timeout;

            make_timeout(&timeout, FLUSH_INTERVAL_MS);

            int rc = pthread_cond_timedwait(&g_not_empty, &g_lock, &timeout);

            if (rc != 0 && rc != ETIMEDOUT) {

                LOG_ERROR("data_writer: "
                          "condition wait failed");

                break;
            }
        }

        /*
         * ==================================================
         * 优先处理 queue
         * ==================================================
         */
        if (g_queue_count > 0) {

            data_batch_t *queued = &g_queue[g_queue_head];

            memcpy(&batch, queued, sizeof(data_batch_t));

            /*
             * queue head 前进。
             */
            g_queue_head++;

            if (g_queue_head >= BATCH_QUEUE_SIZE) {

                g_queue_head = 0;
            }

            g_queue_count--;

            /*
             * 通知生产者：
             *
             * queue 有空间了。
             */
            pthread_cond_signal(&g_not_full);

            pthread_mutex_unlock(&g_lock);

            /*
             * 数据库操作。
             *
             * 此时不持有 g_lock。
             */
            if (flush_to_db(batch.points, batch.count) != 0) {

                LOG_ERROR("data_writer: "
                          "worker flush failed, "
                          "%d points lost",
                          batch.count);
            }

            continue;
        }

        /*
         * ==================================================
         * queue 没数据，处理 active buffer
         * ==================================================
         *
         * 如果：
         *
         * active 有数据
         *
         * 可能是：
         *
         *   1. 1 秒 timeout
         *   2. shutdown
         *   3. 手动 flush
         */
        if (g_active_count > 0) {

            /*
             * 如果 queue 有空间，
             * 直接把 active batch 放进 queue。
             */
            if (g_queue_count < BATCH_QUEUE_SIZE) {

                queue_active_batch_locked();

                pthread_mutex_unlock(&g_lock);

                continue;
            }
        }

        /*
         * ==================================================
         * shutdown
         * ==================================================
         */
        if (g_shutdown_requested) {

            /*
             * 如果 queue 和 active
             * 都没有数据，可以退出。
             */
            if (g_queue_count == 0 && g_active_count == 0) {

                pthread_mutex_unlock(&g_lock);

                break;
            }
        }

        pthread_mutex_unlock(&g_lock);
    }

    LOG_INFO("data_writer: worker stopped");

    return NULL;
}

/**
 * 初始化。
 */
int data_writer_init(void) {
    pthread_mutex_lock(&g_lock);

    if (g_initialized) {

        pthread_mutex_unlock(&g_lock);

        return 0;
    }

    memset(g_active_buf, 0, sizeof(g_active_buf));

    memset(g_queue, 0, sizeof(g_queue));

    g_active_count = 0;

    g_queue_head = 0;
    g_queue_tail = 0;
    g_queue_count = 0;

    g_shutdown_requested = 0;
    g_running = 1;
    g_initialized = 1;

    pthread_mutex_unlock(&g_lock);

    /*
     * 创建 worker。
     */
    int rc = pthread_create(&g_worker, NULL, data_writer_worker, NULL);

    if (rc != 0) {

        pthread_mutex_lock(&g_lock);

        g_running = 0;
        g_initialized = 0;

        pthread_mutex_unlock(&g_lock);

        LOG_ERROR("data_writer: "
                  "pthread_create failed");

        return -1;
    }

    LOG_INFO("data_writer initialized "
             "(batch=%d, queue=%d, interval=%dms)",
             BATCH_SIZE, BATCH_QUEUE_SIZE, FLUSH_INTERVAL_MS);

    return 0;
}

/**
 * 异步写入入口。
 *
 * 外部线程调用这里。
 *
 * 不执行数据库操作。
 */
int data_writer_enqueue(const char *device_id, const char *metric, double value, uint64_t ts) {
    if (!device_id || !metric) {
        return -1;
    }

    pthread_mutex_lock(&g_lock);

    if (!g_initialized || !g_running || g_shutdown_requested) {

        pthread_mutex_unlock(&g_lock);

        return -1;
    }

    /*
     * 如果 active buffer 满了：
     *
     * 将它放入 queue。
     *
     * queue 满则等待 worker 消费。
     */
    while (g_active_count >= BATCH_SIZE) {

        /*
         * 尝试把当前 batch 放进 queue。
         */
        if (g_queue_count < BATCH_QUEUE_SIZE) {

            queue_active_batch_locked();

            break;
        }

        /*
         * queue 满。
         *
         * 这里采用 backpressure：
         *
         * 不丢数据，
         * 等 worker 消费出空间。
         */
        pthread_cond_wait(&g_not_full, &g_lock);

        /*
         * shutdown 期间不要继续写。
         */
        if (g_shutdown_requested) {

            pthread_mutex_unlock(&g_lock);

            return -1;
        }
    }

    /*
     * 当前 active buffer 现在一定有空间。
     */
    data_point_t *point = &g_active_buf[g_active_count];

    /*
     * 防止字符串不带 '\0'。
     */
    snprintf(point->device_id, sizeof(point->device_id), "%s", device_id);

    snprintf(point->metric, sizeof(point->metric), "%s", metric);

    point->value = value;
    point->ts = ts;

    g_active_count++;

    /*
     * 达到 BATCH_SIZE：
     *
     * 立即放进 queue。
     */
    if (g_active_count >= BATCH_SIZE) {

        if (g_queue_count < BATCH_QUEUE_SIZE) {

            queue_active_batch_locked();
        }
    }

    /*
     * 唤醒 worker。
     */
    pthread_cond_signal(&g_not_empty);

    pthread_mutex_unlock(&g_lock);

    return 0;
}

/**
 * 手动 flush。
 *
 * 注意：
 *
 * 这里是异步 flush。
 *
 * 只负责把 active batch 放进 queue，
 * 不等待数据库完成。
 */
void data_writer_flush(void) {
    pthread_mutex_lock(&g_lock);

    if (!g_initialized || !g_running) {

        pthread_mutex_unlock(&g_lock);

        return;
    }

    /*
     * 如果 active 有数据，
     * 尝试放入 queue。
     */
    while (g_active_count > 0 && g_queue_count >= BATCH_QUEUE_SIZE) {

        /*
         * queue 满了。
         *
         * 等 worker 消费。
         */
        pthread_cond_wait(&g_not_full, &g_lock);

        if (g_shutdown_requested) {

            pthread_mutex_unlock(&g_lock);

            return;
        }
    }

    if (g_active_count > 0) {

        queue_active_batch_locked();
    }

    pthread_cond_signal(&g_not_empty);

    pthread_mutex_unlock(&g_lock);
}

/**
 * 当前待处理数据量。
 *
 * 包括：
 *
 *   active buffer
 *   queue 中的 batch
 */
int data_writer_pending(void) {
    pthread_mutex_lock(&g_lock);

    int pending = g_active_count + g_queue_count * BATCH_SIZE;

    pthread_mutex_unlock(&g_lock);

    return pending;
}

/**
 * shutdown。
 *
 * 保证：
 *
 *   active
 *   queue
 *
 * 中的数据都交给 worker。
 */
void data_writer_shutdown(void) {
    pthread_mutex_lock(&g_lock);

    if (!g_initialized) {

        pthread_mutex_unlock(&g_lock);

        return;
    }

    /*
     * 如果 active 还有数据，
     * 先放进 queue。
     */
    while (g_active_count > 0 && g_queue_count >= BATCH_QUEUE_SIZE) {

        pthread_cond_wait(&g_not_full, &g_lock);
    }

    if (g_active_count > 0) {

        queue_active_batch_locked();
    }

    /*
     * 请求 worker 退出。
     */
    g_shutdown_requested = 1;

    /*
     * 唤醒 worker。
     */
    pthread_cond_broadcast(&g_not_empty);

    pthread_mutex_unlock(&g_lock);

    /*
     * 等 worker：
     *
     *   queue 清空
     *   active 清空
     *   DB 操作完成
     *   worker 退出
     */
    pthread_join(g_worker, NULL);

    pthread_mutex_lock(&g_lock);

    g_running = 0;
    g_initialized = 0;

    pthread_mutex_unlock(&g_lock);

    LOG_INFO("data_writer shutdown");
}