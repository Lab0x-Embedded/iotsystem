/**
 * @file sql_escape.h
 *
 * SQL 字符串参数统一转义 (防注入 + 防转义写出缓冲)
 *
 * 约定:
 *   - 任何会拼进 SQL 单引号字面量的外部输入, 都必须先经这里转义
 *   - 缓冲用 SQL_ESC_CAP(max_len) 声明, max_len 取目标列宽 (如 device_id 为 64)
 *   - 输入超长直接失败, **绝不截断**: mysql_real_escape_string 最坏写出 2*len+1 字节,
 *     输入长度不受控就会写出固定栈缓冲
 *
 * 用法:
 *   char esc_id[SQL_ESC_CAP(64)];
 *   if (sql_escape_conn(conn, esc_id, sizeof(esc_id), device_id) != 0)
 *       return -1;                       // 参数非法或超长
 *   snprintf(sql, sizeof(sql), "SELECT ... WHERE device_id='%s'", esc_id);
 */
#ifndef E2_SQL_ESCAPE_H
#define E2_SQL_ESCAPE_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/** 转义缓冲所需最小容量 (max_len = 输入允许的最长字节数) */
#define SQL_ESC_CAP(max_len)  ((max_len) * 2 + 1)

/**
 * 转义 src (结果不含首尾单引号).
 *  @param mysql  连接句柄 (db_pool_get_mysql 的返回值)
 *  @param dst    输出缓冲
 *  @param cap    dst 容量, 建议 SQL_ESC_CAP(max_len)
 *  @param src    输入 (可为 NULL, 视为空串)
 *  @return 0 成功; -1 参数非法, 或 2*strlen(src)+1 > cap (此时不写入 dst)
 */
int sql_escape(void *mysql, char *dst, size_t cap, const char *src);

/**
 * 同 sql_escape, 但直接从池连接取句柄 (调用方不必持有 MYSQL*).
 *  @param conn   db_conn_t* (取 void* 以匹配 db_pool 的调用约定)
 *  @return 0 成功; -1 参数非法 / 超长 / 连接无效
 */
int sql_escape_conn(void *conn, char *dst, size_t cap, const char *src);

#ifdef __cplusplus
}
#endif

#endif /* E2_SQL_ESCAPE_H */
