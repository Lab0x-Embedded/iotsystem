/**
 * @file db_pool.c
 *
 * P5 MySQL 连接池 — 真实实现
 */
#include "data/db_pool.h"
#include "common/log.h"

#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <mysql.h>
#include <stdbool.h>

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

/** 创建单条MySQL连接 */
static MYSQL *create_mysql_conn(const db_pool_config_t *cfg) {
    MYSQL *mysql = mysql_init(NULL);
    if (!mysql) {
        LOG_ERROR("mysql_init failed");
        return NULL;
    }

    // 设置连接选项
    bool reconnect = 1;
    mysql_options(mysql, MYSQL_OPT_RECONNECT, &reconnect);
    mysql_options(mysql, MYSQL_SET_CHARSET_NAME, "utf8mb4");

    // 连接数据库
    if (!mysql_real_connect(mysql, cfg->host, cfg->user, cfg->password,
                           cfg->database, cfg->port, NULL, 0)) {
        LOG_ERROR("mysql_real_connect failed: %s", mysql_error(mysql));
        mysql_close(mysql);
        return NULL;
    }

    // 设置字符集
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

    // 初始化MySQL库
    if (mysql_library_init(0, NULL, NULL)) {
        LOG_ERROR("mysql_library_init failed");
        free(g_pool);
        g_pool = NULL;
        return -1;
    }

    // 创建连接池
    int success_count = 0;
    for (int i = 0; i < n; i++) {
        g_pool[i].in_use = 0;
        g_pool[i].index = i;
        g_pool[i].config = *cfg;  // 复制配置
        g_pool[i].mysql = create_mysql_conn(cfg);
        if (g_pool[i].mysql) {
            success_count++;
        }
    }

    if (success_count == 0) {
        LOG_ERROR("db_pool_init: no connections created");
        mysql_library_end();
        free(g_pool);
        g_pool = NULL;
        return -1;
    }

    g_pool_size = n;
    LOG_INFO("db_pool initialized (size=%d, connected=%d)", n, success_count);
    return 0;
}

void db_pool_shutdown(void) {
    if (!g_pool) return;

    pthread_mutex_lock(&g_lock);
    for (int i = 0; i < g_pool_size; i++) {
        if (g_pool[i].mysql) {
            mysql_close(g_pool[i].mysql);
            g_pool[i].mysql = NULL;
        }
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
        for (int i = 0; i < g_pool_size; i++) {
            if (!g_pool[i].in_use) {
                g_pool[i].in_use = 1;
                pthread_mutex_unlock(&g_lock);
                
                // 检查连接是否有效
                if (db_pool_ping(&g_pool[i]) != 0) {
                    if (reconnect_conn(&g_pool[i]) != 0) {
                        g_pool[i].in_use = 0;
                        pthread_cond_signal(&g_avail);
                        continue;
                    }
                }
                
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
    if (!conn || !conn->mysql || !sql) return -1;

    if (mysql_query(conn->mysql, sql) != 0) {
        LOG_ERROR("mysql_query failed: %s", mysql_error(conn->mysql));
        return -1;
    }
    return 0;
}

void *db_pool_query(db_conn_t *conn, const char *sql) {
    if (!conn || !conn->mysql || !sql) return NULL;

    if (mysql_query(conn->mysql, sql) != 0) {
        LOG_ERROR("mysql_query failed: %s", mysql_error(conn->mysql));
        return NULL;
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
