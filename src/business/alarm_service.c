/**
 * @file alarm_service.c
 *
 * P4 alarm - MySQL-backed
 */
#include "business/alarm_service.h"
#include <mysql.h>
#include "data/db_pool.h"
#include "data/sql_escape.h"
#include "common/log.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>

static int op_from_str(const char *s) {
    if (!s) return ALARM_OP_GT;
    if (strcmp(s,"lt")==0) return ALARM_OP_LT;
    if (strcmp(s,"eq")==0) return ALARM_OP_EQ;
    if (strcmp(s,"gte")==0) return ALARM_OP_GTE;
    if (strcmp(s,"lte")==0) return ALARM_OP_LTE;
    return ALARM_OP_GT;
}

static const char *severity_to_str(alarm_severity_t sev) {
    switch (sev) {
        case ALARM_SEVERITY_CRITICAL: return "critical";
        case ALARM_SEVERITY_WARN: return "warning";
        case ALARM_SEVERITY_INFO: return "info";
    }
    return "warning";
}

static alarm_severity_t severity_from_str(const char *s) {
    if (!s) return ALARM_SEVERITY_WARN;
    if (strcmp(s,"critical")==0) return ALARM_SEVERITY_CRITICAL;
    if (strcmp(s,"info")==0) return ALARM_SEVERITY_INFO;
    return ALARM_SEVERITY_WARN;
}

static int alarm_hit(alarm_compare_t op, double v, double t) {
    switch (op) {
        case ALARM_OP_GT: return v>t;
        case ALARM_OP_LT: return v<t;
        case ALARM_OP_EQ: return v==t;
        case ALARM_OP_GTE: return v>=t;
        case ALARM_OP_LTE: return v<=t;
    }
    return 0;
}

/* 连续检测：超阈值 N 次才告警，回落自动 resolve */
#define CONSECUTIVE_THRESHOLD 3
#define MAX_TRACKED 256
static struct {
    char device_id[ALARM_DEV_LEN];
    char metric[ALARM_METRIC_LEN];
    int  count;
} g_tracked[MAX_TRACKED];
static int g_tracked_n = 0;

static int alarm_find_tracked(const char *device_id, const char *metric) {
    for (int i = 0; i < g_tracked_n; i++) {
        if (strcmp(g_tracked[i].device_id, device_id) == 0 &&
            strcmp(g_tracked[i].metric, metric) == 0) return i;
    }
    return -1;
}

/* auto-resolve: 复用外部连接版本，用于事件循环流水线 */
static void alarm_auto_resolve_with_conn(db_conn_t *conn, const char *device_id, const char *metric) {
    if (!conn) return;
    char esc_id[SQL_ESC_CAP(64)];
    char esc_metric[SQL_ESC_CAP(64)];
    if (sql_escape_conn(conn, esc_id, sizeof(esc_id), device_id) != 0 ||
        sql_escape_conn(conn, esc_metric, sizeof(esc_metric), metric) != 0) {
        LOG_WARN("ALERT auto-resolve skipped: device_id/metric too long");
        return;
    }
    char sql[512];
    snprintf(sql, sizeof(sql),
        "UPDATE alerts SET status='resolved', resolved_at=NOW() "
        "WHERE device_id='%s' AND metric='%s' AND status='active'",
        esc_id, esc_metric);
    int rc = db_pool_exec(conn, sql);
    if (rc != 0)
        LOG_ERROR("ALERT auto-resolve FAILED: %s %s", device_id, metric);
    else
        LOG_WARN("ALERT auto-resolve executed: %s %s", device_id, metric);
    /* 回落清零计数 */
    int idx = alarm_find_tracked(device_id, metric);
    if (idx >= 0) g_tracked[idx].count = 0;
}

static int alarm_consecutive_check(const char *device_id, const char *metric) {
    int idx = alarm_find_tracked(device_id, metric);
    if (idx < 0) {
        if (g_tracked_n >= MAX_TRACKED) return 0;
        idx = g_tracked_n++;
        strncpy(g_tracked[idx].device_id, device_id, ALARM_DEV_LEN - 1);
        strncpy(g_tracked[idx].metric, metric, ALARM_METRIC_LEN - 1);
        g_tracked[idx].count = 0;
    }
    g_tracked[idx].count++;
    if (g_tracked[idx].count >= CONSECUTIVE_THRESHOLD) {
        g_tracked[idx].count = 0;
        return 1;  /* 达到阈值，允许告警 */
    }
    return 0;  /* 次数不够，跳过 */
}

int alarm_service_init(void) {
    LOG_INFO("alarm_service initialized (MySQL)");
    return 0;
}

int alarm_add_rule(const char *device_id, const char *metric,
                   alarm_compare_t op, double threshold,
                   alarm_severity_t severity) {
    if (!device_id || !metric) return -1;
    db_conn_t *conn = db_pool_get();
    if (!conn) return -1;
    char esc_id[SQL_ESC_CAP(64)];
    char esc_metric[SQL_ESC_CAP(64)];
    if (sql_escape_conn(conn, esc_id, sizeof(esc_id), device_id) != 0 ||
        sql_escape_conn(conn, esc_metric, sizeof(esc_metric), metric) != 0) {
        LOG_WARN("alarm_add_rule: device_id/metric too long");
        db_pool_put(conn);
        return -1;
    }
    char sql[1024];
    snprintf(sql,sizeof(sql),
        "INSERT INTO alert_rules (rule_name,device_id,metric,condition_type,threshold,severity)"
        "VALUES('%s > %.2f','%s','%s','%s',%.2f,'%s')",
        esc_metric, threshold, esc_id, esc_metric,
        op==ALARM_OP_GT?"gt":op==ALARM_OP_LT?"lt":op==ALARM_OP_GTE?"gte":op==ALARM_OP_LTE?"lte":"eq",
        threshold, severity_to_str(severity));
    int rc = db_pool_exec(conn, sql);
    db_pool_put(conn);
    if (rc) LOG_ERROR("alarm_add_rule failed");
    return rc ? -1 : 0;
}

/* 评估一条上报数据（复用外部连接，避免事件循环多连接死锁） */
void alarm_evaluate_with_conn(void *vconn, const char *device_id, const char *metric, double value) {
    if (!device_id || !metric || !vconn) return;
    db_conn_t *conn = (db_conn_t *)vconn;
    char esc_id[SQL_ESC_CAP(64)];
    char esc_metric[SQL_ESC_CAP(64)];
    if (sql_escape_conn(conn, esc_id, sizeof(esc_id), device_id) != 0 ||
        sql_escape_conn(conn, esc_metric, sizeof(esc_metric), metric) != 0) {
        LOG_WARN("ALERT evaluate skipped: device_id/metric too long");
        return;
    }
    char sql[512];
    snprintf(sql, sizeof(sql),
        "SELECT rule_id,condition_type,threshold,severity FROM alert_rules "
        "WHERE enabled=1 AND metric='%s' AND (device_id='%s' OR device_id IS NULL OR device_id='')",
        esc_metric, esc_id);
    void *res = db_pool_query(conn, sql);
    if (!res) return;
    MYSQL_ROW row;
    MYSQL_RES *R = (MYSQL_RES*)res;
    while ((row = mysql_fetch_row(R))) {
        uint64_t rule_id = row[0] ? strtoull(row[0], NULL, 10) : 0;
        const char *rs = row[1]?row[1]:"gt";
        double t = row[2]?atof(row[2]):0.0;
        const char *rv = row[3]?row[3]:"warning";
        (void)rule_id;
        if (!alarm_hit(op_from_str(rs), value, t)) {
            /* 温度回落到正常范围，自动 resolve 旧告警 */
            alarm_auto_resolve_with_conn(conn, device_id, metric);
            continue;
        }

        /* === 去重检查：同一规则是否已有 active 告警 === */
        char dedup_sql[512];
        snprintf(dedup_sql, sizeof(dedup_sql),
            "SELECT id FROM alerts "
            "WHERE device_id='%s' AND metric='%s' AND rule_id=%llu AND status='active' "
            "LIMIT 1",
            esc_id, esc_metric, (unsigned long long)rule_id);
        void *dedup_res = db_pool_query(conn, dedup_sql);
        if (dedup_res) {
            MYSQL_ROW dedup_row = mysql_fetch_row((MYSQL_RES*)dedup_res);
            db_pool_free_result(dedup_res);
            if (dedup_row) {
                /* 已有 active 告警，跳过 */
                LOG_DEBUG("ALERT dedup: %s %s=%.2f (already active)", device_id, metric, value);
                continue;
            }
        }

        /* === 连续检测：超阈值 N 次才告警 === */
        if (!alarm_consecutive_check(device_id, metric)) {
            LOG_DEBUG("ALERT consecutive: %s %s=%.2f (counting)", device_id, metric, value);
            continue;
        }

        char sql2[512];
        snprintf(sql2,sizeof(sql2),
            "INSERT INTO alerts (rule_id,device_id,metric,current_value,threshold,severity,status)"
            "VALUES(%llu,'%s','%s',%.2f,%.2f,'%s','active')",
            (unsigned long long)rule_id, esc_id, esc_metric, value, t, rv);
        db_pool_exec(conn, sql2);
        LOG_WARN("ALERT: %s %s=%.2f > %.2f", device_id, metric, value, t);
    }
    db_pool_free_result(res);
}

void alarm_evaluate(const char *device_id, const char *metric, double value) {
    if (!device_id || !metric) return;
    db_conn_t *conn = db_pool_get();
    if (!conn) return;
    alarm_evaluate_with_conn(conn, device_id, metric, value);
    db_pool_put(conn);
}

int alarm_recent(alarm_record_t *out, int max_n) {
    if (!out || max_n<=0) return 0;
    db_conn_t *conn = db_pool_get();
    if (!conn) return 0;
    char sql[512];
    snprintf(sql,sizeof(sql),
        "SELECT a.id,a.device_id,a.metric,a.current_value,a.threshold,a.severity,a.status,"
        "a.acknowledged_by,COALESCE(u1.display_name,''),a.acknowledged_at,"
        "a.resolved_by,COALESCE(u2.display_name,''),a.resolved_at,"
        "a.created_at"
        " FROM alerts a"
        " LEFT JOIN users u1 ON a.acknowledged_by=u1.id"
        " LEFT JOIN users u2 ON a.resolved_by=u2.id"
        " ORDER BY a.id DESC LIMIT %d", max_n);
    void *res = db_pool_query(conn, sql);
    if (!res) { db_pool_put(conn); return 0; }
    int n=0; MYSQL_ROW row; MYSQL_RES *R=(MYSQL_RES*)res;
    while ((row=mysql_fetch_row(R)) && n<max_n) {
        alarm_record_t *a=&out[n];
        memset(a,0,sizeof(*a));
        a->id = row[0]?strtoull(row[0],NULL,10):0;
        strncpy(a->device_id, row[1]?row[1]:"", ALARM_DEV_LEN-1);
        strncpy(a->metric, row[2]?row[2]:"", ALARM_METRIC_LEN-1);
        a->value = row[3]?atof(row[3]):0.0;
        a->threshold = row[4]?atof(row[4]):0.0;
        a->severity = severity_from_str(row[5]);
        a->acknowledged = (row[6] && (strcmp(row[6],"acknowledged")==0||strcmp(row[6],"resolved")==0)) ? 1 : 0;
        a->acknowledged_by = row[7]?atoi(row[7]):0;
        strncpy(a->acknowledged_by_name, row[8]?row[8]:"", ALARM_NAME_LEN-1);
        strncpy(a->acknowledged_at, row[9]?row[9]:"", 31);
        a->resolved_by = row[10]?atoi(row[10]):0;
        strncpy(a->resolved_by_name, row[11]?row[11]:"", ALARM_NAME_LEN-1);
        strncpy(a->resolved_at, row[12]?row[12]:"", 31);
        /* 触发时间 = 告警产生时间 (alerts.created_at)。
         * 此前这里写 time(NULL)：每次查询都把触发时间刷成"现在"，
         * 无论多老的告警都显示"刚刚"。 */
        {
            struct tm tm_v;
            memset(&tm_v, 0, sizeof(tm_v));
            a->triggered_at = time(NULL);   /* 兜底: 行损坏时退化为当前时间 */
            if (row[13] &&
                strptime(row[13], "%Y-%m-%d %H:%M:%S", &tm_v) != NULL) {
                tm_v.tm_isdst = -1;
                a->triggered_at = mktime(&tm_v);
            }
        }
        n++;
    }
    db_pool_free_result(res); db_pool_put(conn);
    return n;
}

int alarm_count(void) {
    db_conn_t *conn = db_pool_get();
    if (!conn) return 0;
    void *res = db_pool_query(conn,"SELECT COUNT(*) FROM alerts");
    int n=0;
    if (res) {
        MYSQL_ROW row = mysql_fetch_row((MYSQL_RES*)res);
        if (row && row[0]) n = atoi(row[0]);
        db_pool_free_result(res);
    }
    db_pool_put(conn);
    return n;
}

int alarm_acknowledge(uint64_t id, int user_id) {
    if (id==0) return -1;
    db_conn_t *conn = db_pool_get();
    if (!conn) return -1;
    char sql[256];
    snprintf(sql,sizeof(sql),
        "UPDATE alerts SET status='acknowledged',acknowledged_at=NOW(),"
        "acknowledged_by=%d WHERE id=%llu AND status='active'",
        user_id, (unsigned long long)id);
    int rc = db_pool_exec(conn, sql);
    db_pool_put(conn);
    if (rc) return -1;
    LOG_INFO("alarm acknowledged id=%llu by user=%d", (unsigned long long)id, user_id);
    return 0;
}

int alarm_resolve(uint64_t id, int user_id) {
    if (id==0) return -1;
    db_conn_t *conn = db_pool_get();
    if (!conn) return -1;
    char sql[256];
    snprintf(sql,sizeof(sql),
        "UPDATE alerts SET status='resolved',resolved_at=NOW(),"
        "resolved_by=%d WHERE id=%llu AND status IN ('active','acknowledged')",
        user_id, (unsigned long long)id);
    int rc = db_pool_exec(conn, sql);
    db_pool_put(conn);
    if (rc) return -1;
    LOG_INFO("alarm resolved id=%llu by user=%d", (unsigned long long)id, user_id);
    return 0;
}

int alarm_query_rules(alarm_rule_config_t *out, int max_n) {
    if (!out || max_n <= 0) return 0;
    db_conn_t *conn = db_pool_get();
    if (!conn) return 0;
    void *res = db_pool_query(conn,
        "SELECT rule_id,device_id,metric,condition_type,threshold,severity,enabled"
        " FROM alert_rules ORDER BY rule_id DESC");
    if (!res) { db_pool_put(conn); return 0; }
    int n = 0;
    MYSQL_ROW row;
    MYSQL_RES *R = (MYSQL_RES*)res;
    while ((row = mysql_fetch_row(R)) && n < max_n) {
        alarm_rule_config_t *r = &out[n];
        memset(r, 0, sizeof(*r));
        r->id = row[0] ? strtoull(row[0], NULL, 10) : 0;
        strncpy(r->device_id, row[1] ? row[1] : "", ALARM_DEV_LEN - 1);
        strncpy(r->metric, row[2] ? row[2] : "", ALARM_METRIC_LEN - 1);
        r->op = op_from_str(row[3]);
        r->threshold = row[4] ? atof(row[4]) : 0.0;
        r->severity = severity_from_str(row[5]);
        r->enabled = row[6] ? atoi(row[6]) : 1;
        n++;
    }
    db_pool_free_result(res);
    db_pool_put(conn);
    return n;
}

int alarm_toggle_rule(uint64_t rule_id) {
    if (rule_id == 0) return -1;
    db_conn_t *conn = db_pool_get();
    if (!conn) return -1;
    char sql[256];
    snprintf(sql, sizeof(sql),
        "UPDATE alert_rules SET enabled = NOT enabled WHERE rule_id=%llu",
        (unsigned long long)rule_id);
    int rc = db_pool_exec(conn, sql);
    db_pool_put(conn);
    if (rc) {
        LOG_ERROR("alarm_toggle_rule failed id=%llu", (unsigned long long)rule_id);
        return -1;
    }
    LOG_INFO("alarm_toggle_rule id=%llu", (unsigned long long)rule_id);
    return 0;
}

int alarm_edit_rule(uint64_t rule_id, const char *device_id, const char *metric,
                    alarm_compare_t op, double threshold,
                    alarm_severity_t severity) {
    if (rule_id == 0 || !metric) return -1;
    db_conn_t *conn = db_pool_get();
    if (!conn) return -1;
    const char *op_str = op == ALARM_OP_GT ? "gt" : op == ALARM_OP_LT ? "lt" :
                         op == ALARM_OP_EQ ? "eq" : op == ALARM_OP_GTE ? "gte" : "lte";
    char esc_id[SQL_ESC_CAP(64)];
    char esc_metric[SQL_ESC_CAP(64)];
    if (sql_escape_conn(conn, esc_id, sizeof(esc_id), device_id ? device_id : "") != 0 ||
        sql_escape_conn(conn, esc_metric, sizeof(esc_metric), metric) != 0) {
        LOG_WARN("alarm_edit_rule: device_id/metric too long");
        db_pool_put(conn);
        return -1;
    }
    char sql[1024];
    snprintf(sql, sizeof(sql),
        "UPDATE alert_rules SET device_id='%s', metric='%s', condition_type='%s', "
        "threshold=%.2f, severity='%s' WHERE rule_id=%llu",
        esc_id, esc_metric, op_str, threshold,
        severity == ALARM_SEVERITY_CRITICAL ? "critical" :
        severity == ALARM_SEVERITY_INFO ? "info" : "warning",
        (unsigned long long)rule_id);
    int rc = db_pool_exec(conn, sql);
    db_pool_put(conn);
    if (rc) {
        LOG_ERROR("alarm_edit_rule failed id=%llu", (unsigned long long)rule_id);
        return -1;
    }
    LOG_INFO("alarm_edit_rule id=%llu", (unsigned long long)rule_id);
    return 0;
}

int alarm_delete_rule(uint64_t rule_id) {
    if (rule_id == 0) return -1;
    db_conn_t *conn = db_pool_get();
    if (!conn) return -1;
    char sql[256];
    snprintf(sql, sizeof(sql),
        "DELETE FROM alert_rules WHERE rule_id=%llu",
        (unsigned long long)rule_id);
    int rc = db_pool_exec(conn, sql);
    db_pool_put(conn);
    if (rc) {
        LOG_ERROR("alarm_delete_rule failed id=%llu", (unsigned long long)rule_id);
        return -1;
    }
    LOG_INFO("alarm_delete_rule id=%llu", (unsigned long long)rule_id);
    return 0;
}
