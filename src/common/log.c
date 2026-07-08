/**
 * @file log.c
 */
#include "common/log.h"
#include <stdarg.h>
#include <time.h>
#include <pthread.h>
static log_level_t g_level = LOG_LEVEL_INFO;
static pthread_mutex_t g_mtx = PTHREAD_MUTEX_INITIALIZER;
static const char *ls(log_level_t l) {
    switch (l) { case LOG_LEVEL_DEBUG: return "DBG"; case LOG_LEVEL_INFO:  return "INF"; case LOG_LEVEL_WARN:  return "WRN"; case LOG_LEVEL_ERROR: return "ERR"; }
    return "???";
}
void log_init(log_level_t l){g_level=l;}
void log_set_level(log_level_t l){g_level=l;}
static void emit(log_level_t lvl, const char *fmt, va_list ap){
    if(lvl<g_level) return;
    struct timespec ts; clock_gettime(CLOCK_REALTIME,&ts);
    struct tm tm; localtime_r(&ts.tv_sec, &tm);
    char t[32]; strftime(t, sizeof(t), "%H:%M:%S", &tm);
    pthread_mutex_lock(&g_mtx);
    fprintf(stderr, "%s.%03ld [%.3s] ", t, ts.tv_nsec / 1000000L, ls(lvl));
    vfprintf(stderr, fmt, ap); fputc('\n', stderr);
    pthread_mutex_unlock(&g_mtx);
}
void log_debug(const char *f,...){va_list a;va_start(a,f);emit(LOG_LEVEL_DEBUG,f,a);va_end(a);}
void log_info (const char *f,...){va_list a;va_start(a,f);emit(LOG_LEVEL_INFO ,f,a);va_end(a);}
void log_warn (const char *f,...){va_list a;va_start(a,f);emit(LOG_LEVEL_WARN ,f,a);va_end(a);}
void log_error(const char *f,...){va_list a;va_start(a,f);emit(LOG_LEVEL_ERROR,f,a);va_end(a);}
