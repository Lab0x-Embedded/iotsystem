/**
 * @file data_writer.c
 *
 * P5 批量异步写入 — 内存缓冲 + 桩实现
 */
#include "data/data_writer.h"
#include "data/shard_router.h"
#include "data/db_pool.h"
#include "common/log.h"

#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#define BATCH_SIZE 64

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

int data_writer_init(void) {
    pthread_mutex_lock(&g_lock);
    memset(g_buf, 0, sizeof(g_buf));
    g_head = 0;
    g_count = 0;
    pthread_mutex_unlock(&g_lock);
    LOG_INFO("data_writer initialized (batch=%d) [stub]", BATCH_SIZE);
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
    if (n > 0) LOG_DEBUG("data_writer flush %d points [stub]", n);
}

int data_writer_pending(void) {
    int n;
    pthread_mutex_lock(&g_lock);
    n = g_count;
    pthread_mutex_unlock(&g_lock);
    return n;
}
