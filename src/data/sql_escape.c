/**
 * @file sql_escape.c
 *
 * SQL 字符串参数统一转义 — 见 include/data/sql_escape.h
 */
#include "data/sql_escape.h"
#include "data/db_pool.h"

#include <mysql.h>
#include <string.h>

int sql_escape(void *mysql, char *dst, size_t cap, const char *src) {
    size_t n;

    if (!mysql || !dst || cap == 0)
        return -1;

    n = src ? strlen(src) : 0;
    if (n * 2 + 1 > cap)          /* 超长: 拒绝而不是截断 */
        return -1;

    mysql_real_escape_string((MYSQL *)mysql, dst, src ? src : "", (unsigned long)n);
    return 0;
}

int sql_escape_conn(void *conn, char *dst, size_t cap, const char *src) {
    if (!conn)
        return -1;
    return sql_escape(db_pool_get_mysql((db_conn_t *)conn), dst, cap, src);
}
