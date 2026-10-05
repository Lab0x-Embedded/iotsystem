/**
 * @file log.c
 *
 * 线程安全的结构化日志输出到 stderr。
 * 级别: DEBUG < INFO < WARN < ERROR。
 */
#include "log.h"
#include <stdarg.h>
#include <time.h>
#include <pthread.h>
#include <stdio.h>

static log_level_t g_level = LOG_LEVEL_INFO;
static pthread_mutex_t g_mtx = PTHREAD_MUTEX_INITIALIZER;

static const char *level_str(log_level_t lvl) {
    switch (lvl) {
        case LOG_LEVEL_DEBUG: return "DBG";
        case LOG_LEVEL_INFO:  return "INF";
        case LOG_LEVEL_WARN:  return "WRN";
        case LOG_LEVEL_ERROR: return "ERR";
        default:              return "???";
    }
}

void log_init(log_level_t lvl) {
    g_level = lvl;
}

void log_set_level(log_level_t lvl) {
    g_level = lvl;
}

static void emit(log_level_t lvl, const char *fmt, va_list ap) {
    if (lvl < g_level) return;

    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);

    struct tm tm;
    localtime_r(&ts.tv_sec, &tm);

    char ts_buf[32];
    strftime(ts_buf, sizeof(ts_buf), "%H:%M:%S", &tm);

    pthread_mutex_lock(&g_mtx);
    fprintf(stderr, "%s.%03ld [%.3s] ",
            ts_buf, ts.tv_nsec / 1000000L, level_str(lvl));
    vfprintf(stderr, fmt, ap);
    fputc('\n', stderr);
    pthread_mutex_unlock(&g_mtx);
}

void log_debug(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    emit(LOG_LEVEL_DEBUG, fmt, ap);
    va_end(ap);
}

void log_info(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    emit(LOG_LEVEL_INFO, fmt, ap);
    va_end(ap);
}

void log_warn(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    emit(LOG_LEVEL_WARN, fmt, ap);
    va_end(ap);
}

void log_error(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    emit(LOG_LEVEL_ERROR, fmt, ap);
    va_end(ap);
}
