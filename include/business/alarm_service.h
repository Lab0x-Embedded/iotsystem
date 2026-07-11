/**
 * @file alarm_service.h
 *
 * P4 告警处理 — 规则引擎 + 告警记录
 *
 * 设计:
 *   - 规则: 设备 + 指标 + 阈值 + 比较符
 *   - 触发: 上报数据命中规则 → 写入 alarm_log
 *   - 通知: 回调 (可扩展为 webhook/邮件)
 */
#ifndef E2_ALARM_SERVICE_H
#define E2_ALARM_SERVICE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ALARM_DEV_LEN   65
#define ALARM_METRIC_LEN 64
#define ALARM_MSG_LEN   256
#define MAX_RULES       64
#define MAX_ALARMS      1024

typedef enum {
    ALARM_OP_GT = 0,
    ALARM_OP_LT,
    ALARM_OP_EQ,
    ALARM_OP_GTE,
    ALARM_OP_LTE,
} alarm_compare_t;

typedef enum {
    ALARM_SEVERITY_INFO = 0,
    ALARM_SEVERITY_WARN,
    ALARM_SEVERITY_CRITICAL,
} alarm_severity_t;

typedef struct {
    int            enabled;
    char           device_id[ALARM_DEV_LEN];
    char           metric[ALARM_METRIC_LEN];
    alarm_compare_t op;
    double         threshold;
    alarm_severity_t severity;
} alarm_rule_t;

typedef struct {
    uint64_t       id;
    char           device_id[ALARM_DEV_LEN];
    char           metric[ALARM_METRIC_LEN];
    double         value;
    double         threshold;
    alarm_severity_t severity;
    char           message[ALARM_MSG_LEN];
    uint64_t       triggered_at;
    int            acknowledged;
} alarm_record_t;

int  alarm_service_init(void);

/** 添加规则; 返回规则索引, -1 失败. */
int  alarm_add_rule(const char *device_id, const char *metric,
                    alarm_compare_t op, double threshold,
                    alarm_severity_t severity);

/** 评估一条上报数据; 命中则生成告警. */
void alarm_evaluate(const char *device_id, const char *metric, double value);

/** 读取最近 N 条告警. */
int  alarm_recent(alarm_record_t *out, int max_n);

/** 当前告警总数. */
int  alarm_count(void);

/** 按 id 确认告警; 返回 0 成功. */
int  alarm_acknowledge(uint64_t id);

#ifdef __cplusplus
}
#endif

#endif /* E2_ALARM_SERVICE_H */
