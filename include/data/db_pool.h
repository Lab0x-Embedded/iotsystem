/**
 * @file db_pool.h
 *
 * P5 MySQL 连接池 — 固定大小 + 互斥分配
 *
 * 设计:
 *   - 启动时创建 N 条连接
 *   - db_pool_get / db_pool_put 分配/归还
 *   - 连接失效时自动重连
 */
#ifndef E2_DB_POOL_H
#define E2_DB_POOL_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct db_conn db_conn_t;

/** 连接池配置 */
typedef struct {
    const char *host;
    uint16_t    port;
    const char *user;
    const char *password;
    const char *database;
    int         pool_size;
} db_pool_config_t;

int  db_pool_init(const db_pool_config_t *cfg);
void db_pool_shutdown(void);

/** 获取一条连接 (阻塞直到有可用). */
db_conn_t *db_pool_get(void);

/** 归还连接. */
void db_pool_put(db_conn_t *conn);

/** 执行查询 (简化封装). */
int db_pool_exec(db_conn_t *conn, const char *sql);

/** 执行查询并获取结果集. */
void *db_pool_query(db_conn_t *conn, const char *sql);

/** 释放结果集. */
void db_pool_free_result(void *result);

/** 获取连接的MySQL句柄 (用于prepared statements). */
void *db_pool_get_mysql(db_conn_t *conn);

/** 检查连接是否有效. */
int db_pool_ping(db_conn_t *conn);

#ifdef __cplusplus
}
#endif

#endif /* E2_DB_POOL_H */
