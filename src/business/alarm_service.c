/**
 * @file alarm_service.c
 *
 * P4 alarm - MySQL-backed
 */
#include "business/alarm_service.h"
#include <mysql.h>
#include "data/db_pool.h"
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
    char sql[1024];
    snprintf(sql,sizeof(sql),
        "INSERT INTO alert_rules (rule_name,device_id,metric,condition_type,threshold,severity)"
        "VALUES('\%s > %.2f','%s','%s','%s',%.2f,'%s')",
        metric, threshold, device_id, metric,
        op==ALARM_OP_GT?"gt":op==ALARM_OP_LT?"lt":op==ALARM_OP_GTE?"gte":op==ALARM_OP_LTE?"lte":"eq",
        threshold, severity_to_str(severity));
    int rc = db_pool_exec(conn, sql);
    db_pool_put(conn);
    if (rc) LOG_ERROR("alarm_add_rule failed");
    return rc ? -1 : 0;
}

void alarm_evaluate(const char *device_id, const char *metric, double value) {
    if (!device_id || !metric) return;
    db_conn_t *conn = db_pool_get();
    if (!conn) return;
    void *res = db_pool_query(conn, "SELECT device_id,metric,condition_type,threshold,severity FROM alert_rules WHERE enabled=1");
    if (!res) { db_pool_put(conn); return; }
    MYSQL_ROW row;
    MYSQL_RES *R = (MYSQL_RES*)res;
    while ((row = mysql_fetch_row(R))) {
        const char *rd = row[0]?row[0]:"";
        const char *rm = row[1]?row[1]:"";
        const char *rs = row[2]?row[2]:"gt";
        const char *rt = row[3]?row[3]:"0";
        const char *rv = row[4]?row[4]:"warning";
        if (strlen(rd)>0 && strcmp(rd,device_id)!=0) continue;
        if (strcmp(rm,metric)!=0) continue;
        double t = atof(rt);
        if (!alarm_hit(op_from_str(rs), value, t)) continue;
        char sql[512];
        snprintf(sql,sizeof(sql),
            "INSERT INTO alerts (device_id,metric,current_value,threshold,severity,status)"
            "VALUES('%s','%s',%.2f,%.2f,'%s','active')",
            device_id, metric, value, t, rv);
        db_pool_exec(conn, sql);
        LOG_WARN("ALERT: %s %s=%.2f > %.2f", device_id, metric, value, t);
    }
    db_pool_free_result(res);
    db_pool_put(conn);
}

int alarm_recent(alarm_record_t *out, int max_n) {
    if (!out || max_n<=0) return 0;
    db_conn_t *conn = db_pool_get();
    if (!conn) return 0;
    char sql[256];
    snprintf(sql,sizeof(sql),
        "SELECT id,device_id,metric,current_value,threshold,severity,status"
        " FROM alerts ORDER BY id DESC LIMIT %d", max_n);
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
        a->triggered_at = time(NULL);
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

int alarm_acknowledge(uint64_t id) {
    if (id==0) return -1;
    db_conn_t *conn = db_pool_get();
    if (!conn) return -1;
    char sql[256];
    snprintf(sql,sizeof(sql),
        "UPDATE alerts SET status='acknowledged',acknowledged_at=NOW()"
        " WHERE id=%llu AND status='active'", (unsigned long long)id);
    int rc = db_pool_exec(conn, sql);
    db_pool_put(conn);
    if (rc) return -1;
    LOG_INFO("alarm acknowledged id=%llu", (unsigned long long)id);
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
    char sql[1024];
    snprintf(sql, sizeof(sql),
        "UPDATE alert_rules SET device_id='%s', metric='%s', condition_type='%s', "
        "threshold=%.2f, severity='%s' WHERE rule_id=%llu",
        device_id ? device_id : "", metric, op_str, threshold,
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
