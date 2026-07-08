/**
 * @file alarm_service.c
 *
 * P4 告警处理 — 内存实现
 */
#include "business/alarm_service.h"
#include "common/log.h"

#include <stdlib.h>
#include <string.h>
#include <pthread.h>

static alarm_rule_t  g_rules[MAX_RULES];
static int           g_rule_count = 0;

static alarm_record_t g_alarms[MAX_ALARMS];
static int            g_alarm_head = 0;
static int            g_alarm_count = 0;
static uint64_t       g_alarm_seq = 0;

static pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;

int alarm_service_init(void) {
    pthread_mutex_lock(&g_lock);
    memset(g_rules, 0, sizeof(g_rules));
    g_rule_count = 0;
    memset(g_alarms, 0, sizeof(g_alarms));
    g_alarm_head = 0;
    g_alarm_count = 0;
    g_alarm_seq = 0;
    pthread_mutex_unlock(&g_lock);
    LOG_INFO("alarm_service initialized");
    return 0;
}

int alarm_add_rule(const char *device_id, const char *metric,
                   alarm_compare_t op, double threshold,
                   alarm_severity_t severity) {
    if (!device_id || !metric) return -1;
    pthread_mutex_lock(&g_lock);
    if (g_rule_count >= MAX_RULES) { pthread_mutex_unlock(&g_lock); return -1; }
    alarm_rule_t *r = &g_rules[g_rule_count];
    r->enabled = 1;
    strncpy(r->device_id, device_id, ALARM_DEV_LEN - 1);
    strncpy(r->metric, metric, ALARM_METRIC_LEN - 1);
    r->op = op;
    r->threshold = threshold;
    r->severity = severity;
    int idx = g_rule_count++;
    pthread_mutex_unlock(&g_lock);
    LOG_INFO("alarm rule added: %s %s op=%.2f", device_id, metric, threshold);
    return idx;
}

static int alarm_hit(alarm_compare_t op, double value, double threshold) {
    switch (op) {
        case ALARM_OP_GT:  return value > threshold;
        case ALARM_OP_LT:  return value < threshold;
        case ALARM_OP_EQ:  return value == threshold;
        case ALARM_OP_GTE: return value >= threshold;
        case ALARM_OP_LTE: return value <= threshold;
    }
    return 0;
}

void alarm_evaluate(const char *device_id, const char *metric, double value) {
    if (!device_id || !metric) return;
    pthread_mutex_lock(&g_lock);
    for (int i = 0; i < g_rule_count; i++) {
        alarm_rule_t *r = &g_rules[i];
        if (!r->enabled) continue;
        if (strcmp(r->device_id, device_id) != 0 && strcmp(r->device_id, "*") != 0) continue;
        if (strcmp(r->metric, metric) != 0) continue;
        if (!alarm_hit(r->op, value, r->threshold)) continue;
        /* 生成告警 */
        alarm_record_t *a = &g_alarms[g_alarm_head];
        g_alarm_head = (g_alarm_head + 1) % MAX_ALARMS;
        if (g_alarm_count < MAX_ALARMS) g_alarm_count++;
        a->id = ++g_alarm_seq;
        strncpy(a->device_id, device_id, ALARM_DEV_LEN - 1);
        strncpy(a->metric, metric, ALARM_METRIC_LEN - 1);
        a->value = value;
        a->severity = r->severity;
        snprintf(a->message, ALARM_MSG_LEN, "%s=%.2f hit rule (op=%d, threshold=%.2f)",
                 metric, value, (int)r->op, r->threshold);
        a->triggered_at = (uint64_t)time(NULL);
        LOG_WARN("ALARM: %s", a->message);
    }
    pthread_mutex_unlock(&g_lock);
}

int alarm_recent(alarm_record_t *out, int max_n) {
    if (!out || max_n <= 0) return 0;
    pthread_mutex_lock(&g_lock);
    int n = g_alarm_count < max_n ? g_alarm_count : max_n;
    for (int i = 0; i < n; i++) {
        int idx = (g_alarm_head - 1 - i + MAX_ALARMS) % MAX_ALARMS;
        out[i] = g_alarms[idx];
    }
    pthread_mutex_unlock(&g_lock);
    return n;
}

int alarm_count(void) {
    int n;
    pthread_mutex_lock(&g_lock);
    n = g_alarm_count;
    pthread_mutex_unlock(&g_lock);
    return n;
}
