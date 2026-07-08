/**
 * @file log.h
 */
#ifndef E2_LOG_H
#define E2_LOG_H
#include <stdio.h>
typedef enum {
    LOG_LEVEL_DEBUG = 0,
    LOG_LEVEL_INFO,
    LOG_LEVEL_WARN,
    LOG_LEVEL_ERROR,
} log_level_t;
void log_init(log_level_t lvl);
void log_set_level(log_level_t lvl);
void log_debug(const char *fmt, ...);
void log_info (const char *fmt, ...);
void log_warn (const char *fmt, ...);
void log_error(const char *fmt, ...);
#define LOG_DEBUG(...) log_debug(__VA_ARGS__)
#define LOG_INFO(...)  log_info(__VA_ARGS__)
#define LOG_WARN(...)  log_warn(__VA_ARGS__)
#define LOG_ERROR(...) log_error(__VA_ARGS__)
#endif /* E2_LOG_H */
