/**
 * @file mqtt_broker.c
 *
 * Phase 2 基础 + Phase 3 PUBLISH/UNSUBSCRIBE:
 *   - CONNECT    → 解析 client_id / username / password, 数据库认证
 *   - SUBSCRIBE  → 记录订阅 topic, 回复 SUBACK
 *   - UNSUBSCRIBE→ 移除订阅 topic, 回复 UNSUBACK
 *   - PUBLISH    → 按 topic 分发给所有匹配的在线订阅者 (QoS 0)
 *   - PINGREQ    → PINGRESP
 *   - DISCONNECT → 标记连接关闭
 */
#include "mqtt_broker.h"
#include "mqtt/mqtt_parser.h"
#include "mqtt/mqtt_codec.h"
#include "mqtt/mqtt_topic.h"
#include "business/command_service.h"
#include "business/device_manager.h"
#include "server/thread_pool.h"

#include "common/log.h"
#include <cJSON.h>
#include "business/alarm_service.h"
#include "data/db_pool.h"
#include "data/sql_escape.h"

#include <mysql.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <netinet/in.h>
#include <errno.h>
#include <pthread.h>

extern thread_pool_t *g_thread_pool;

/* 前向声明 */
static int send_bytes(int fd, const uint8_t *buf, uint32_t len);
static void send_packet(int fd, mqtt_packet_t *pkt);

/* ------------------------------------------------------------------ */
/* 全局 session 表 / 在线连接表                                          */
/* ------------------------------------------------------------------ */
#define MAX_SESSIONS 128
#define MAX_CONNS   256

static mqtt_session_t    g_sessions[MAX_SESSIONS];
static mqtt_connection_t *g_conns[MAX_CONNS];
static int g_conn_count;
static uint16_t g_pkt_id;
static pthread_mutex_t g_conn_mutex = PTHREAD_MUTEX_INITIALIZER;

void mqtt_broker_init(void) {
    memset(g_sessions, 0, sizeof(g_sessions));
    memset(g_conns, 0, sizeof(g_conns));
    g_conn_count = 0;
    g_pkt_id = 1;
    cmd_mgr_init();
}

void mqtt_broker_register(mqtt_connection_t *conn) {
    if (!conn) return;
    if (g_conn_count >= MAX_CONNS) {
        LOG_ERROR("connection table full, can't register fd=%d", conn->fd);
        return;
    }
    conn->will_topic[0] = '\0';
    conn->will_payload = NULL;
    conn->will_payload_len = 0;
    pthread_mutex_lock(&g_conn_mutex);
    g_conns[g_conn_count++] = conn;
    pthread_mutex_unlock(&g_conn_mutex);
    LOG_INFO("register conn fd=%d (total=%d)", conn->fd, g_conn_count);
}

void mqtt_broker_unregister(mqtt_connection_t *conn) {
    if (!conn) return;

    /* 触发 Will Message (断连时发布, 只发一次) */
    if (conn->will_topic[0] != '\0' && conn->will_payload) {
        LOG_INFO("will publish topic=%s payload_len=%u",
                 conn->will_topic, conn->will_payload_len);
        /* 构建 PUBLISH raw payload: [topic_len][topic][will_payload] */
        uint16_t tlen = (uint16_t)strlen(conn->will_topic);
        uint32_t raw_len = 2 + tlen + conn->will_payload_len;
        uint8_t *raw = malloc(raw_len);
        if (raw) {
            raw[0] = (uint8_t)(tlen >> 8);
            raw[1] = (uint8_t)(tlen & 0xFF);
            memcpy(raw + 2, conn->will_topic, tlen);
            memcpy(raw + 2 + tlen, conn->will_payload, conn->will_payload_len);
        } else {
            raw = conn->will_payload;
            raw_len = conn->will_payload_len;
        }
        /* 构造 PUBLISH 并遍历所有订阅者 */
        for (int ci = 0; ci < g_conn_count; ci++) {
            mqtt_connection_t *target = g_conns[ci];
            if (!target || !target->connected || target == conn) continue;
            for (int si = 0; si < target->sub_count; si++) {
                if (mqtt_topic_match(target->subs[si].topic, conn->will_topic)) {
                    mqtt_packet_t fwd = {0};
                    fwd.fix_header.type  = MQTT_PUBLISH;
                    fwd.fix_header.flags = 0;
                    fwd.payload     = raw;
                    fwd.payload_len = raw_len;
                    send_packet(target->fd, &fwd);
                    LOG_INFO("  will -> fwd to fd=%d sub='%s'",
                             target->fd, target->subs[si].topic);
                    break;
                }
            }
        }
        if (raw != conn->will_payload) free(raw);
        free(conn->will_payload);
        conn->will_payload = NULL;
        conn->will_payload_len = 0;
        conn->will_topic[0] = '\0';
    }

    pthread_mutex_lock(&g_conn_mutex);
    for (int i = 0; i < g_conn_count; i++) {
        if (g_conns[i] == conn) {
            g_conns[i] = g_conns[--g_conn_count];
            pthread_mutex_unlock(&g_conn_mutex);
            LOG_INFO("unregister conn fd=%d (total=%d)", conn->fd, g_conn_count);
            return;
        }
    }
    pthread_mutex_unlock(&g_conn_mutex);
}

/* ------------------------------------------------------------------ */
/* 辅助函数                                                            */
/* ------------------------------------------------------------------ */
static uint32_t read_str(const uint8_t *p, uint32_t len, char *out, size_t cap) {
    if (len < 2) return 0;
    uint16_t slen = ((uint16_t)p[0] << 8) | p[1];
    if (slen + 2 > len) slen = (uint16_t)(len - 2);
    if (slen + 1 > cap) slen = (uint16_t)(cap - 1);
    memcpy(out, p + 2, slen);
    out[slen] = '\0';
    return 2 + slen;
}

static int send_bytes(int fd, const uint8_t *buf, uint32_t len) {
    int eagain_count = 0;
    while (len > 0) {
        ssize_t n = send(fd, buf, len, 0);
        if (n > 0) { buf += n; len -= (uint32_t)n; continue; }
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            if (++eagain_count > 100000) {
                LOG_ERROR("send_bytes EAGAIN exhausted fd=%d", fd);
                return -1;
            }
            continue;
        }
        if (errno == EINTR) continue;
        return -1;
    }
    return 0;
}
static void send_packet(int fd, mqtt_packet_t *pkt) {
    mqtt_buf_t b = mqtt_encode(pkt);
    if (b.data) {
        send_bytes(fd, b.data, b.len);
        free(b.data);
    }

}
/* ------------------------------------------------------------------ */
/* 外部 API — find_conn / send_cmd / 离线重放                           */
/* ------------------------------------------------------------------ */
mqtt_connection_t *mqtt_broker_find_conn(const char *client_id) {
    if (!client_id) return NULL;
    pthread_mutex_lock(&g_conn_mutex);
    for (int i = 0; i < g_conn_count; i++) {
        mqtt_connection_t *conn = g_conns[i];
        if (conn && conn->connected && strcmp(conn->client_id, client_id) == 0) {
            pthread_mutex_unlock(&g_conn_mutex);
            return conn;
        }
    }
    pthread_mutex_unlock(&g_conn_mutex);
    return NULL;
}

int mqtt_broker_send_cmd(mqtt_connection_t *conn,
                         const char *topic,
                         const uint8_t *app_payload,
                         uint32_t app_len,
                         uint16_t *out_pid) {
    if (!conn || !conn->connected) return -1;

    uint16_t pid = g_pkt_id++;
    uint16_t tlen = (uint16_t)strlen(topic);
    uint32_t raw_len = 2 + tlen + 2 + app_len;
    uint8_t *raw = malloc(raw_len);
    if (!raw) return -1;

    /* 构建 PUBLISH raw payload: topic_len + topic + packet_id + app_data */
    raw[0] = (uint8_t)(tlen >> 8);
    raw[1] = (uint8_t)(tlen & 0xFF);
    memcpy(raw + 2, topic, tlen);
    raw[2 + tlen]     = (uint8_t)(pid >> 8);
    raw[2 + tlen + 1] = (uint8_t)(pid & 0xFF);
    memcpy(raw + 2 + tlen + 2, app_payload, app_len);

    mqtt_packet_t pkt = {0};
    pkt.fix_header.type  = MQTT_PUBLISH;
    pkt.fix_header.flags = 0x02;  /* QoS 1, DUP=0, Retain=0 */
    pkt.payload     = raw;
    pkt.payload_len = raw_len;

    send_packet(conn->fd, &pkt);
    free(raw);

    if (out_pid) *out_pid = pid;
    LOG_INFO("send_cmd fd=%d topic=%s pid=%u app_len=%u",
             conn->fd, topic, pid, app_len);
    return 0;
}

/* 设备重连后, 重放离线命令 */
static void replay_offline_commands(mqtt_connection_t *conn) {
    if (!conn || !conn->connected) return;
    offline_cmd_t *list = cmd_mgr_dequeue_all(conn->client_id);
    if (!list) return;
    LOG_INFO("replay %d offline cmds for client=%s", 0, conn->client_id);
    /* count first */
    int count = 0;
    for (offline_cmd_t *c = list; c; c = c->entry.stqe_next) count++;
    LOG_INFO("replay %d offline cmds for client=%s", count, conn->client_id);

    for (offline_cmd_t *cur = list; cur; cur = cur->entry.stqe_next) {
        /* 构建应用层 JSON: {"id":"cmd_xxx","cmd":"set_temp","payload":{...}} */
        /* payload_len = len("{\"id\":\"") + id + len("\",\"cmd\":\"") + cmd + len("\",\"payload\":") + payload + "}" */
        char buf[CMD_BODY_LEN];
        int n = snprintf(buf, sizeof(buf),
                         "{\"id\":\"%s\",\"cmd\":\"%s\",\"payload\":%s}",
                         cur->command_id, cur->cmd_name, cur->cmd_payload);
        if (n <= 0 || (size_t)n >= sizeof(buf)) {
            LOG_WARN("replay cmd too long, skip");
            continue;
        }

        char topic[128];
        snprintf(topic, sizeof(topic), "cmd/%s/exec", conn->client_id);

        uint16_t pid = 0;
        mqtt_broker_send_cmd(conn, topic,
                             (const uint8_t *)buf, (uint32_t)n, &pid);
        LOG_INFO("  replayed cmd=%s id=%s pid=%u", cur->cmd_name, cur->command_id, pid);
    }
    cmd_mgr_free_offline_list(list);
}

/* ------------------------------------------------------------------ */
/* CONNECT                                                              */
/* ------------------------------------------------------------------ */
static void handle_connect(mqtt_connection_t *conn, mqtt_packet_t *pkt) {
    /* 协议层检查
     * 注意: 解析器按 MQTT 3.1.1 的定长可变头实现(协议名固定 4 字节 "MQTT"、
     * keepalive 之后直接是 Client ID)。MQTT 3.1(协议名 "MQIsdp") 与 MQTT 5
     * (keepalive 后有 properties) 的布局不同，仅放宽这里的判断会导致报文解析错位，
     * 所以仍要求 3.1.1(level=4)，但给出可操作的提示。 */
    if (pkt->vh.connect.protocol_level != 4) {
        LOG_ERROR("unsupported MQTT protocol level %d (broker only supports 3.1.1 / level 4; "
                  "please set your client to MQTT 3.1.1)",
                  pkt->vh.connect.protocol_level);
        goto refused;
    }
    if (memcmp(pkt->vh.connect.protocol_name, "MQTT", 4) != 0) {
        LOG_ERROR("bad protocol name: %.8s", pkt->vh.connect.protocol_name);
        goto refused;
    }
    /* 解析 CLIENT ID */
    uint32_t remain = pkt->payload_len;
    uint32_t off;
    uint16_t cid_len;
    char client_id[MQTT_ID_MAX];
    char username[MQTT_ID_MAX] = {0};
    char password[MQTT_ID_MAX] = {0};
    uint8_t cflags;

    if (remain < 10) {
        goto refused;
    }
    off = 10;

    cid_len = ((uint16_t)pkt->payload[off] << 8) | pkt->payload[off + 1];
    off += 2;
    if (off + cid_len > remain) {
        goto refused;
    }
    memset(client_id, 0, sizeof(client_id));
    memcpy(client_id, pkt->payload + off, cid_len);
    client_id[cid_len] = '\0';
    off += cid_len;

    cflags = pkt->vh.connect.connect_flags;

    /* Will Topic / Message (Will Flag = bit3) */
    if (cflags & 0x08) {
        if (off + 2 > remain) {
            goto refused;
        }
        uint16_t wt_len = ((uint16_t)pkt->payload[off] << 8) | pkt->payload[off + 1];
        off += 2;
        if (off + wt_len > remain) {
            goto refused;
        }
        memset(conn->will_topic, 0, sizeof(conn->will_topic));
        size_t wt_copy = wt_len < (sizeof(conn->will_topic) - 1) ? wt_len : (sizeof(conn->will_topic) - 1);
        memcpy(conn->will_topic, pkt->payload + off, wt_copy);
        conn->will_topic[wt_copy] = '\0';
        off += wt_len;

        if (off + 2 > remain) {
            goto refused;
        }
        uint16_t wm_len = ((uint16_t)pkt->payload[off] << 8) | pkt->payload[off + 1];
        off += 2;
        if (off + wm_len > remain) {
            goto refused;
        }
        conn->will_payload = malloc(wm_len);
        if (conn->will_payload) {
            memcpy(conn->will_payload, pkt->payload + off, wm_len);
            conn->will_payload_len = wm_len;
        }
        off += wm_len;
        conn->will_qos = (cflags >> 3) & 0x03;
        LOG_INFO("WILL topic=%s qos=%d payload_len=%u",
                 conn->will_topic, conn->will_qos, conn->will_payload_len);
    }

    if (cflags & 0x80) {
        if (off + 2 > remain) {
            goto refused;
        }
        off += read_str(pkt->payload + off, remain - off, username, sizeof(username));
    }
    if (cflags & 0x40) {
        if (off + 2 > remain) {
            goto refused;
        }
        off += read_str(pkt->payload + off, remain - off, password, sizeof(password));
    }

    LOG_INFO("CONNECT cid=%s user=%s secret=%.4s...", client_id, username, password);

    /* 数据库认证: 查 devices 表校验 product_key + device_secret
     * (username/password 已转义; 顺带取回 device_id 供 presence 标记) */
    int ok = 0;
    char auth_device_id[MQTT_ID_MAX] = {0};
    {
        db_conn_t *db = db_pool_get();
        if (db) {
            char esc_user[SQL_ESC_CAP(128)];
            char esc_pw[SQL_ESC_CAP(128)];
            char auth_sql[768];
            if (sql_escape_conn(db, esc_user, sizeof(esc_user), username) != 0 ||
                sql_escape_conn(db, esc_pw, sizeof(esc_pw), password) != 0) {
                LOG_WARN("CONNECT auth: username/password too long");
                db_pool_put(db);
                goto refused;
            }
            snprintf(auth_sql, sizeof(auth_sql),
                "SELECT device_id FROM devices "
                "WHERE product_key='%s' AND device_secret='%s' "
                "AND status IN ('registered','active') LIMIT 1",
                esc_user, esc_pw);
            MYSQL_RES *result = db_pool_query(db, auth_sql);
            if (result) {
                MYSQL_ROW row = mysql_fetch_row(result);
                if (row) {
                    ok = 1;
                    if (row[0])
                        snprintf(auth_device_id, sizeof(auth_device_id), "%s", row[0]);
                }
                db_pool_free_result(result);
            }
            db_pool_put(db);
        }
    }
    if (!ok) {
        LOG_ERROR("auth failed for user=%s", username);
        goto refused;
    }

    /* 同 client_id 的旧连接: 踢掉。
     * ESP8266 等模组断线重连时 client_id 不变，若不处理，旧连接会一直挂在
     * g_conns 里(直到 keepalive 超时)，导致 find_conn 命中已死的连接。 */
    {
        mqtt_connection_t *old = mqtt_broker_find_conn(client_id);
        if (old && old != conn) {
            LOG_INFO("kick duplicate client_id=%s: close old fd=%d", client_id, old->fd);
            old->connected = 0;
            shutdown(old->fd, SHUT_RDWR);
        }
    }

    /* 存储 session */
    conn->connected = 1;
    conn->authenticated = 1;
    conn->keepalive = pkt->vh.connect.keepalive;
    conn->last_active = time(NULL);
    strncpy(conn->client_id, client_id, sizeof(conn->client_id) - 1);
    strncpy(conn->product_key, username, sizeof(conn->product_key) - 1);
    strncpy(conn->device_id, auth_device_id, sizeof(conn->device_id) - 1);

    /* 标记在线 (非阻塞: 交给 device_manager 的 presence 线程批量下刷) */
    if (conn->device_id[0])
        device_manager_presence(conn->device_id, true);

    /* CONNACK ACCEPTED */
    {
        mqtt_packet_t ack = {0};
        ack.fix_header.type = MQTT_CONNACK;
        ack.fix_header.flags = 0;
        ack.fix_header.remain_len = 2;
        ack.vh.connack.session_present = 0;
        ack.vh.connack.return_code   = MQTT_CONNACK_ACCEPTED;
        send_packet(conn->fd, &ack);
    }
    return;

refused:
    {
        mqtt_packet_t ack = {0};
        ack.fix_header.type  = MQTT_CONNACK;
        ack.fix_header.flags = 0;
        ack.fix_header.remain_len = 2;
        ack.vh.connack.session_present = 0;
        ack.vh.connack.return_code   = MQTT_CONNACK_REFUSED_BAD_CREDS;
        send_packet(conn->fd, &ack);
    }
}

/* ------------------------------------------------------------------ */
/* SUBSCRIBE                                                            */
/* ------------------------------------------------------------------ */
static void handle_subscribe(mqtt_connection_t *conn, mqtt_packet_t *pkt) {
    uint8_t *data = pkt->payload; uint32_t len = pkt->payload_len;
    if (len < 2) return;
    uint16_t packet_id = ((uint16_t)data[0] << 8) | data[1];
    uint32_t off = 2;
    uint8_t return_codes[8];
    int rc_count = 0;

    while (off + 3 <= len && conn->sub_count < MAX_SUBS_PER_CONN) {
        uint16_t tlen = ((uint16_t)data[off] << 8) | data[off + 1];
        off += 2;
        if (off + tlen + 1 > len) break;
        uint8_t qos = data[off + tlen];
        char *topic = (char *)(data + off);
        char tmp[65];
        memset(tmp, 0, sizeof(tmp));
        size_t copy_len = tlen < 64 ? tlen : 64;
        memcpy(tmp, topic, copy_len);
        tmp[copy_len] = '\0';
        off += tlen + 1;

        /* 校验 topic 合法性 */
        if (!mqtt_topic_valid(tmp, 1)) {
            LOG_WARN("SUB invalid topic='%s'", tmp);
            return_codes[rc_count++] = 0x80; /* 拒绝 */
            continue;
        }

        /* 存储 subscription */
        mqtt_subscription_t *sub = &conn->subs[conn->sub_count++];
        strncpy(sub->topic, tmp, sizeof(sub->topic) - 1);
        sub->qos = qos;
        return_codes[rc_count++] = qos;
        LOG_INFO("SUB #%d topic=%s qos=%d", conn->sub_count, tmp, qos);
    }

    /* SUBACK */
    {
        mqtt_packet_t ack = {0};
        ack.fix_header.type       = MQTT_SUBACK;
        ack.fix_header.flags      = 0;
        ack.fix_header.remain_len = 2 + rc_count;
        ack.vh.id.packet_id       = packet_id;
        ack.payload               = return_codes;
        ack.payload_len           = rc_count;
        send_packet(conn->fd, &ack);
    }
    /* 订阅后重放离线命令 */
    replay_offline_commands(conn);
}

/* ------------------------------------------------------------------ */
/* UNSUBSCRIBE                                                         */
/* ------------------------------------------------------------------ */
static void handle_unsubscribe(mqtt_connection_t *conn, mqtt_packet_t *pkt) {
    uint8_t *data = pkt->payload; uint32_t len = pkt->payload_len;
    if (len < 2) return;
    uint16_t packet_id = ((uint16_t)data[0] << 8) | data[1];
    uint32_t off = 2;
    int removed = 0;

    while (off + 2 <= len) {
        uint16_t tlen = ((uint16_t)data[off] << 8) | data[off + 1];
        off += 2;
        if (off + tlen > len) break;
        char tmp[65];
        memset(tmp, 0, sizeof(tmp));
        size_t copy_len = tlen < 64 ? tlen : 64;
        memcpy(tmp, data + off, copy_len);
        tmp[copy_len] = '\0';
        off += tlen;

        /* 从 subscription 列表移除 */
        for (int i = 0; i < conn->sub_count; i++) {
            if (strcmp(conn->subs[i].topic, tmp) == 0) {
                /* 用最后一个覆盖 */
                conn->subs[i] = conn->subs[--conn->sub_count];
                removed++;
                LOG_INFO("UNSUB topic='%s' (removed=%d)", tmp, removed);
                break;
            }
        }
    }

    /* UNSUBACK */
    {
        mqtt_packet_t ack = {0};
        ack.fix_header.type       = MQTT_UNSUBACK;
        ack.fix_header.flags      = 0;
        ack.fix_header.remain_len = 2;
        ack.vh.id.packet_id       = packet_id;
        send_packet(conn->fd, &ack);
    }
    (void)removed;
}

/* ------------------------------------------------------------------ */
/* PUBLISH: 主线程做协议解析 + 订阅转发; 重 DB/告警 任务下沉线程池           */
/* ------------------------------------------------------------------ */

typedef struct {
    char product_key[65];
    char device_id[65];
    char metric[64];
    double value;
    uint64_t ts;
} publish_task_t;

static void publish_worker(void *arg) {
    publish_task_t *task = (publish_task_t *)arg;

    db_conn_t *db = db_pool_get();
    if (!db) {
        free(task);
        return;
    }

    /* 转义 MQTT 上报里的 product_key / device_id / metric (payload 由设备控制, 必须转义) */
    char esc_pk[SQL_ESC_CAP(65)];
    char esc_id[SQL_ESC_CAP(65)];
    char esc_metric[SQL_ESC_CAP(64)];
    if (sql_escape_conn(db, esc_pk, sizeof(esc_pk), task->product_key) != 0 ||
        sql_escape_conn(db, esc_id, sizeof(esc_id), task->device_id) != 0 ||
        sql_escape_conn(db, esc_metric, sizeof(esc_metric), task->metric) != 0) {
        LOG_WARN("publish: product_key/device_id/metric too long, point dropped");
        db_pool_put(db);
        free(task);
        return;
    }

    /* 自动注册 product */
    char ensure_sql[768];
    snprintf(ensure_sql, sizeof(ensure_sql),
        "INSERT IGNORE INTO products (product_key, product_name) "
        "VALUES ('%s','MQTT Auto Registered')",
        esc_pk);
    db_pool_exec(db, ensure_sql);

    /* 自动注册 device; 已存在则顺带刷新 presence (不额外增加往返) */
    snprintf(ensure_sql, sizeof(ensure_sql),
        "INSERT INTO devices (product_key, device_id, device_name, status, online, last_online) "
        "VALUES ('%s','%s','%s','active',TRUE,NOW()) "
        "ON DUPLICATE KEY UPDATE online=TRUE, last_online=NOW()",
        esc_pk, esc_id, esc_id);
    if (db_pool_exec(db, ensure_sql) != 0) {
        LOG_WARN("auto-register device failed: %s", task->device_id);
    }

    /* 告警评估（复用当前连接，避免多连接死锁） */
    alarm_evaluate_with_conn(db, task->device_id, task->metric, task->value);

    /* 写入 data_reports_YYYYMM */
    time_t now = time(NULL);
    struct tm tm_buf;
    struct tm *tm_now = localtime_r(&now, &tm_buf);
    char tbl[64];
    snprintf(tbl, sizeof(tbl), "data_reports_%04d%02d",
             tm_now->tm_year + 1900, tm_now->tm_mon + 1);

    char sql[512];
    snprintf(sql, sizeof(sql),
        "CREATE TABLE IF NOT EXISTS `%s` LIKE data_records_template", tbl);
    if (db_pool_exec(db, sql) != 0) {
        LOG_WARN("CREATE TABLE %s failed (may already exist)", tbl);
    }

    char *ins = (char *)malloc(768);
    if (ins) {
        snprintf(ins, 768,
            "INSERT INTO `%s` (device_id, metric, value, ts) "
            "VALUES ('%s','%s',%.2f,%llu)",
            tbl, esc_id, esc_metric, task->value,
            (unsigned long long)task->ts);
        if (db_pool_exec(db, ins) != 0) {
            LOG_WARN("DB insert failed: %s %s=%.2f",
                     task->device_id, task->metric, task->value);
        }
        free(ins);
    }

    /* 更新 device_latest_data */
    char *upsert = (char *)malloc(768);
    if (upsert) {
        snprintf(upsert, 768,
            "INSERT INTO device_latest_data (device_id, metric, value, ts, updated_at) "
            "VALUES ('%s','%s',%.2f,%llu,NOW()) "
            "ON DUPLICATE KEY UPDATE value=VALUES(value), ts=VALUES(ts), updated_at=NOW()",
            esc_id, esc_metric, task->value,
            (unsigned long long)task->ts);
        db_pool_exec(db, upsert);
        free(upsert);
    }

    db_pool_put(db);
    free(task);
}

static void handle_publish(mqtt_connection_t *conn, mqtt_packet_t *pkt) {
    uint8_t *data = pkt->payload; uint32_t len = pkt->payload_len;
    if (len < 2) return;

    /* 解析 topic */
    uint16_t tlen = ((uint16_t)data[0] << 8) | data[1];
    if (tlen + 2 > len) return;
    char topic[128];
    memset(topic, 0, sizeof(topic));
    size_t copy_len = tlen < (sizeof(topic) - 1) ? tlen : (sizeof(topic) - 1);
    memcpy(topic, data + 2, copy_len);
    topic[copy_len] = '\0';

    /* PUBLISH topic 校验 (不允许通配符) */
    if (!mqtt_topic_valid(topic, 0)) {
        LOG_WARN("PUBLISH invalid topic='%s'", topic);
        return;
    }

    LOG_INFO("PUBLISH from fd=%d topic=%s payload_len=%u",
             conn->fd, topic, len - 2 - tlen);

    /* 主线程: 解析 JSON 并提交后台任务 + SSE 广播 */
    {
        uint32_t payload_off = 2 + tlen;
        uint32_t payload_len = len - payload_off;
        if (payload_len > 0) {
            char *json_str = (char *)malloc(payload_len + 1);
            if (json_str) {
                memcpy(json_str, data + payload_off, payload_len);
                json_str[payload_len] = '\0';

                cJSON *root = cJSON_Parse(json_str);
                if (root) {
                    const cJSON *id = cJSON_GetObjectItem(root, "device_id");
                    const cJSON *dp = cJSON_GetObjectItem(root, "datapoints");
                    if (id && id->valuestring && dp && cJSON_IsArray(dp)) {
                        cJSON *item;
                        cJSON_ArrayForEach(item, dp) {
                            const cJSON *metric = cJSON_GetObjectItem(item, "metric");
                            const cJSON *value = cJSON_GetObjectItem(item, "value");
                            const cJSON *ts   = cJSON_GetObjectItem(item, "ts");
                            if (metric && metric->valuestring && value) {
                                double val = value->valuedouble;
                                uint64_t ts_val = ts ? (uint64_t)ts->valuedouble : (uint64_t)time(NULL);

                                /* 把 DB+告警 任务提交到线程池 */
                                if (g_thread_pool) {
                                    publish_task_t *task = (publish_task_t *)calloc(1, sizeof(publish_task_t));
                                    if (task) {
                                        strncpy(task->product_key, conn->product_key, sizeof(task->product_key) - 1);
                                        strncpy(task->device_id, id->valuestring, sizeof(task->device_id) - 1);
                                        strncpy(task->metric, metric->valuestring, sizeof(task->metric) - 1);
                                        task->value = val;
                                        task->ts = ts_val;
                                        if (thread_pool_submit(g_thread_pool, publish_worker, task) != 0) {
                                            LOG_WARN("thread_pool_submit failed, drop task");
                                            free(task);
                                        }
                                    }
                                }
                            }
                        }
                    } else {
                        LOG_WARN("PUBLISH payload missing device_id or datapoints");
                    }
                    cJSON_Delete(root);
                } else {
                    LOG_WARN("PUBLISH payload JSON parse failed");
                }
                free(json_str);
            }
        }
    }

    /* 遍历所有在线连接, 逐 subscription 匹配 (保留在主线程) */
    for (int ci = 0; ci < g_conn_count; ci++) {
        mqtt_connection_t *target = g_conns[ci];
        if (!target || !target->connected) continue;

        for (int si = 0; si < target->sub_count; si++) {
            if (mqtt_topic_match(target->subs[si].topic, topic)) {
                /* 构造转发的 PUBLISH 报文 (保留原 topic + payload) */
                mqtt_packet_t fwd = {0};
                fwd.fix_header.type  = MQTT_PUBLISH;
                fwd.fix_header.flags = 0;  /* QoS 0, no retain */
                fwd.payload     = data;
                fwd.payload_len = len;
                send_packet(target->fd, &fwd);
                LOG_INFO("  -> fwd to fd=%d sub='%s'", target->fd, target->subs[si].topic);
                break; /* 每个连接最多转发一次 (匹配一个 subscription) */
            }
        }
    }

    /* 如果发布者 QoS > 0, 回复 PUBACK (保留在主线程) */
    {
        uint8_t qos = (pkt->fix_header.flags >> 1) & 0x03;
        if (qos > 0) {
            /* 从 payload 解析 packet_id (topic 之后, QoS 1/2 时发方必带) */
            uint16_t packet_id = 0;
            uint32_t payload_off = 2 + tlen;  /* skip topic in payload */
            if (payload_off + 2 <= len) {
                packet_id = ((uint16_t)pkt->payload[payload_off] << 8)
                          | pkt->payload[payload_off + 1];
            }
            mqtt_packet_t ack = {0};
            ack.fix_header.type       = MQTT_PUBACK;
            ack.fix_header.flags      = 0;
            ack.fix_header.remain_len = 2;
            ack.vh.id.packet_id       = packet_id;
            send_packet(conn->fd, &ack);
            LOG_INFO("PUBACK to fd=%d pid=%u", conn->fd, packet_id);
        }
    }
}

/* ------------------------------------------------------------------ */
/* PINGREQ                                                              */
/* ------------------------------------------------------------------ */
static void handle_ping(mqtt_connection_t *conn) {
    mqtt_packet_t ack = {0};
    ack.fix_header.type       = MQTT_PINGRESP;
    ack.fix_header.flags      = 0;
    ack.fix_header.remain_len = 0;
    send_packet(conn->fd, &ack);

    /* PINGREQ = 设备心跳: 非阻塞标记在线, 避免 MQTT 主循环里做 DB 写 */
    if (conn->device_id[0])
        device_manager_presence(conn->device_id, true);
}

/* ------------------------------------------------------------------ */
/* 路由分发                                                             */
/* ------------------------------------------------------------------ */
void mqtt_broker_dispatch(mqtt_connection_t *conn, mqtt_packet_t *pkt) {
    if (!conn || !pkt) return;
    switch (pkt->fix_header.type) {
    case MQTT_CONNECT:     handle_connect(conn, pkt); break;
    case MQTT_SUBSCRIBE:   handle_subscribe(conn, pkt); break;
    case MQTT_UNSUBSCRIBE: handle_unsubscribe(conn, pkt); break;
    case MQTT_PINGREQ:     handle_ping(conn); break;
    case MQTT_PUBLISH:     handle_publish(conn, pkt); break;
    case MQTT_PUBACK:
        LOG_INFO("PUBACK from fd=%d pid=%u", conn->fd, pkt->vh.id.packet_id);
        cmd_mgr_inflight_signal(conn->fd, pkt->vh.id.packet_id);
        break;
    case MQTT_DISCONNECT:  conn->connected = 0; break;
    default:
        LOG_WARN("unsupported msg type %d", pkt->fix_header.type);
        break;
    }
    conn->last_active = time(NULL);
    mqtt_packet_unref(pkt);
}

/* ------------------------------------------------------------------ */
/* 周期性 tick: keepalive 超时检测                                     */
/* ------------------------------------------------------------------ */
void mqtt_broker_tick(time_t now) {
    for (int i = 0; i < g_conn_count; i++) {
        mqtt_connection_t *conn = g_conns[i];
        if (!conn || !conn->connected) continue;
        if (conn->keepalive == 0) continue;  /* 0 = 不主动断开 */

        uint32_t timeout = (uint32_t)conn->keepalive * 3 / 2;  /* 1.5x */
        if (timeout < 5) timeout = 5;  /* 最少 5s */

        if ((uint32_t)(now - conn->last_active) > timeout) {
            LOG_INFO("keepalive timeout fd=%d cid=%s (%us > %us)",
                     conn->fd, conn->client_id,
                     (unsigned)(now - conn->last_active), timeout);
            conn->connected = 0;
        }
    }
}
