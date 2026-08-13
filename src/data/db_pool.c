/**
 * @file db_pool.c
 *
 * MySQL 连接池
 */
#include "data/db_pool.h"
#include "common/log.h"

#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <mysql.h>
#include <stdbool.h>
#include <errno.h>
#include <sys/time.h>

struct db_conn {
    int      in_use;
    int      index;
    MYSQL   *mysql;
    db_pool_config_t config;
};

#define DEFAULT_POOL_SIZE 4

static db_conn_t       *g_pool = NULL;
static int              g_pool_size = 0;
static pthread_mutex_t  g_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t   g_avail = PTHREAD_COND_INITIALIZER;

/* ================================================================
 *  内部工具函数
 * ================================================================ */

/** 深拷贝配置（解决 char* 浅拷贝问题） */
static int copy_config(db_pool_config_t *dst, const db_pool_config_t *src) {
    *dst = *src;  // 先拷贝所有字段（包括 int 等值类型）

    dst->host     = src->host     ? strdup(src->host)     : NULL;
    dst->user     = src->user     ? strdup(src->user)     : NULL;
    dst->password = src->password ? strdup(src->password) : NULL;
    dst->database = src->database ? strdup(src->database) : NULL;

    if ((src->host     && !dst->host)     ||
        (src->user     && !dst->user)     ||
        (src->password && !dst->password) ||
        (src->database && !dst->database)) {

        free((void *)dst->host);
        free((void *)dst->user);
        free((void *)dst->password);
        free((void *)dst->database);
        return -1;
    }
    return 0;
}

/** 释放深拷贝的配置 */
static void free_config(db_pool_config_t *cfg) {
    free((void *)cfg->host);
    free((void *)cfg->user);
    free((void *)cfg->password);
    free((void *)cfg->database);
    cfg->host = cfg->user = cfg->password = cfg->database = NULL;
}

/** 创建单条MySQL连接 */
static MYSQL *create_mysql_conn(const db_pool_config_t *cfg) {
    MYSQL *mysql = mysql_init(NULL);
    if (!mysql) {
        LOG_ERROR("mysql_init failed");
        return NULL;
    }

    mysql_options(mysql, MYSQL_SET_CHARSET_NAME, "utf8mb4");

    if (!mysql_real_connect(mysql, cfg->host, cfg->user, cfg->password,
                           cfg->database, cfg->port, NULL, 0)) {
        LOG_ERROR("mysql_real_connect failed: %s", mysql_error(mysql));
        mysql_close(mysql);
        return NULL;
    }

    mysql_set_character_set(mysql, "utf8mb4");

    LOG_INFO("MySQL connected to %s:%d/%s", cfg->host, cfg->port, cfg->database);
    return mysql;
}

/** 重新连接 */
static int reconnect_conn(db_conn_t *conn) {
    if (conn->mysql) {
        mysql_close(conn->mysql);
        conn->mysql = NULL;
    }

    conn->mysql = create_mysql_conn(&conn->config);
    return conn->mysql ? 0 : -1;
}

/** 判断是否为可恢复的连接断开错误 */
static bool is_disconnect_error(MYSQL *mysql) {
    unsigned int err = mysql_errno(mysql);
    return err == CR_SERVER_LOST || err == CR_SERVER_GONE_ERROR ||
           err == CR_CONN_HOST_ERROR || err == 2006;
}

/* ================================================================
 *  公开 API
 * ================================================================ */
int db_pool_init(const db_pool_config_t *cfg) {
    if (!cfg || !cfg->host || !cfg->user || !cfg->password || !cfg->database) {
        LOG_ERROR("db_pool_init: invalid config");
        return -1;
    }

    int n = cfg->pool_size > 0 ? cfg->pool_size : DEFAULT_POOL_SIZE;

    g_pool = (db_conn_t *)calloc(n, sizeof(db_conn_t));
    if (!g_pool) {
        LOG_ERROR("db_pool_init: alloc failed");
        return -1;
    }

    if (mysql_library_init(0, NULL, NULL)) {
        LOG_ERROR("mysql_library_init failed");
        free(g_pool);
        g_pool = NULL;
        return -1;
    }

    int success_count = 0;
    for (int i = 0; i < n; i++) {
        g_pool[i].in_use = 0;
        g_pool[i].index  = i;

        if (copy_config(&g_pool[i].config, cfg) != 0) {
            LOG_ERROR("db_pool_init: config copy failed for conn[%d]", i);
            continue;
        }

        g_pool[i].mysql = create_mysql_conn(&g_pool[i].config);
        if (g_pool[i].mysql) {
            success_count++;
        }
    }

    if (success_count == 0) {
        LOG_ERROR("db_pool_init: no connections created");

        for (int i = 0; i < n; i++) {
            free_config(&g_pool[i].config);
        }
        free(g_pool);
        g_pool = NULL;
        mysql_library_end();
        return -1;
    }

    g_pool_size = n;
    LOG_INFO("db_pool initialized (size=%d, connected=%d)", n, success_count);
    return 0;
}

void db_pool_shutdown(void) {
    if (!g_pool) return;

    pthread_mutex_lock(&g_lock);

    /*  等待所有使用中的连接归还（最多 10 秒） */
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += 10;

    while (1) {
        int busy = 0;
        for (int i = 0; i < g_pool_size; i++) {
            if (g_pool[i].in_use) busy++;
        }
        if (busy == 0) break;

        LOG_WARN("db_pool_shutdown: %d connections still in use, waiting...", busy);
        int rc = pthread_cond_timedwait(&g_avail, &g_lock, &ts);
        if (rc == ETIMEDOUT) {
            LOG_WARN("db_pool_shutdown: timeout after 10s, force closing");
            break;
        }
    }

    /* 关闭所有连接并释放深拷贝的配置 */
    for (int i = 0; i < g_pool_size; i++) {
        if (g_pool[i].mysql) {
            mysql_close(g_pool[i].mysql);
            g_pool[i].mysql = NULL;
        }
        free_config(&g_pool[i].config);
    }

    free(g_pool);
    g_pool = NULL;
    g_pool_size = 0;
    pthread_mutex_unlock(&g_lock);

    mysql_library_end();
    LOG_INFO("db_pool shutdown");
}

db_conn_t *db_pool_get(void) {
    if (!g_pool) return NULL;

    pthread_mutex_lock(&g_lock);

    while (1) {
        /* 在锁内查找空闲连接 */
        int found = -1;
        for (int i = 0; i < g_pool_size; i++) {
            if (!g_pool[i].in_use) {
                found = i;
                break;
            }
        }

        if (found >= 0) {
            g_pool[found].in_use = 1;
            pthread_mutex_unlock(&g_lock);

            /* ping 检查（锁外执行，不阻塞其他线程） */
            if (db_pool_ping(&g_pool[found]) != 0) {
                if (reconnect_conn(&g_pool[found]) != 0) {
                    /*  修复：重连失败，重新加锁后再修改状态 */
                    pthread_mutex_lock(&g_lock);
                    g_pool[found].in_use = 0;
                    pthread_cond_signal(&g_avail);
                    /* 锁已持有，直接 continue 到下一轮 while */
                    continue;
                }
            }

            return &g_pool[found];
        }

        /* 没有空闲连接，等待（锁已持有） */
        struct timespec ts;
        struct timeval tv;
        gettimeofday(&tv, NULL);
        ts.tv_sec  = tv.tv_sec + 3;
        ts.tv_nsec = tv.tv_usec * 1000;

        int rc = pthread_cond_timedwait(&g_avail, &g_lock, &ts);
        if (rc == ETIMEDOUT) {
            int in_use_count = 0;
            for (int i = 0; i < g_pool_size; i++)
                if (g_pool[i].in_use) in_use_count++;
            LOG_WARN("db_pool: %d/%d in use, timeout waiting 3s",
                     in_use_count, g_pool_size);
            pthread_mutex_unlock(&g_lock);
            return NULL;
        }
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
    if (!conn || !conn->mysql || !sql) return -1;

    if (mysql_query(conn->mysql, sql) != 0) {
        if (!is_disconnect_error(conn->mysql)) {
            LOG_ERROR("mysql_query failed: %s", mysql_error(conn->mysql));
            return -1;
        }
        LOG_WARN("exec: connection lost, reconnecting...");
        if (reconnect_conn(conn) != 0) return -1;
        if (mysql_query(conn->mysql, sql) != 0) {
            LOG_ERROR("mysql_query retry failed: %s", mysql_error(conn->mysql));
            return -1;
        }
    }
    return 0;
}

void *db_pool_query(db_conn_t *conn, const char *sql) {
    if (!conn || !conn->mysql || !sql) return NULL;

    if (mysql_query(conn->mysql, sql) != 0) {
        if (!is_disconnect_error(conn->mysql)) {
            LOG_ERROR("mysql_query failed: %s", mysql_error(conn->mysql));
            return NULL;
        }
        LOG_WARN("query: connection lost, reconnecting...");
        if (reconnect_conn(conn) != 0) return NULL;
        if (mysql_query(conn->mysql, sql) != 0) {
            LOG_ERROR("mysql_query retry failed: %s", mysql_error(conn->mysql));
            return NULL;
        }
    }

    MYSQL_RES *result = mysql_store_result(conn->mysql);
    if (!result && mysql_field_count(conn->mysql) > 0) {
        LOG_ERROR("mysql_store_result failed: %s", mysql_error(conn->mysql));
        return NULL;
    }

    return result;
}

void db_pool_free_result(void *result) {
    if (result) {
        mysql_free_result((MYSQL_RES *)result);
    }
}

void *db_pool_get_mysql(db_conn_t *conn) {
    return conn ? conn->mysql : NULL;
}

int db_pool_ping(db_conn_t *conn) {
    if (!conn || !conn->mysql) return -1;
    return mysql_ping(conn->mysql);
}