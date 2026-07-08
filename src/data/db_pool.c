/**
 * @file db_pool.c
 *
 * P5 MySQL 连接池 — 桩实现 (无 MySQL 时编译通过, 运行时返回 -1)
 */
#include "data/db_pool.h"
#include "common/log.h"

#include <stdlib.h>
#include <string.h>
#include <pthread.h>

struct db_conn {
    int      in_use;
    int      index;
    /* MYSQL *mysql;  // 真实实现时启用 */
};

#define DEFAULT_POOL_SIZE 4

static db_conn_t       *g_pool = NULL;
static int              g_pool_size = 0;
static pthread_mutex_t  g_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t   g_avail = PTHREAD_COND_INITIALIZER;

int db_pool_init(const db_pool_config_t *cfg) {
    int n = cfg && cfg->pool_size > 0 ? cfg->pool_size : DEFAULT_POOL_SIZE;
    g_pool = (db_conn_t *)calloc(n, sizeof(db_conn_t));
    if (!g_pool) return -1;
    for (int i = 0; i < n; i++) {
        g_pool[i].in_use = 0;
        g_pool[i].index = i;
    }
    g_pool_size = n;
    LOG_INFO("db_pool initialized (size=%d) [stub]", n);
    return 0;
}

void db_pool_shutdown(void) {
    if (g_pool) {
        free(g_pool);
        g_pool = NULL;
    }
    g_pool_size = 0;
    LOG_INFO("db_pool shutdown");
}

db_conn_t *db_pool_get(void) {
    if (!g_pool) return NULL;
    pthread_mutex_lock(&g_lock);
    while (1) {
        for (int i = 0; i < g_pool_size; i++) {
            if (!g_pool[i].in_use) {
                g_pool[i].in_use = 1;
                pthread_mutex_unlock(&g_lock);
                return &g_pool[i];
            }
        }
        pthread_cond_wait(&g_avail, &g_lock);
    }
}

void db_pool_put(db_conn_t *conn) {
    if (!conn || !g_pool) return;
    pthread_mutex_lock(&g_lock);
    conn->in_use = 0;
    pthread_cond_signal(&g_avail);
    pthread_mutex_unlock(&g_lock);
}

int db_pool_exec(db_conn_t *conn, const char *sql) {
    (void)conn;
    (void)sql;
    /* 真实实现: mysql_real_query(conn->mysql, sql, strlen(sql)); */
    return -1;
}
