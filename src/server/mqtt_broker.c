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
#include "business/thing_model.h"
#include "data/db_pool.h"
#include "data/sql_escape.h"
#include "data/shard_router.h"

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

/* ------------------------------------------------------------------ */
/* 认证缓存 (KNOWN_ISSUES M3): DB 不可用时兜底                           */
/* ------------------------------------------------------------------ */
/* 设备密钥长期不变, DB 抖动窗口内不应把重连的在线设备全部拒之门外
 * (设备端会把 0x03 当成密钥错误, 盲目重置密钥越描越黑)。
 * 条目只来自真实 DB 认证成功; TTL 10 分钟内允许 DB 故障时重连。
 * 折衷: 期间被禁用/删除的设备最多还能重连成功一次 TTL 窗口。
 * 仅事件循环线程访问 (handle_connect 所在线程), 无需加锁。 */
#define AUTH_CACHE_CAP 128
#define AUTH_CACHE_TTL 600   /* 秒 */

typedef struct {
    char product_key[65];
    char secret[65];
    char device_id[65];
    time_t cached_at;
} auth_cache_ent_t;

static auth_cache_ent_t g_auth_cache[AUTH_CACHE_CAP];
static int              g_auth_cache_n = 0;

static const char *auth_cache_lookup(const char *pk, const char *secret) {
    time_t now = time(NULL);
    for (int i = 0; i < g_auth_cache_n; i++) {
        if (now - g_auth_cache[i].cached_at > AUTH_CACHE_TTL) continue;
        if (strcmp(g_auth_cache[i].product_key, pk) == 0 &&
            strcmp(g_auth_cache[i].secret, secret) == 0)
            return g_auth_cache[i].device_id;
    }
    return NULL;
}

static void auth_cache_store(const char *pk, const char *secret, const char *device_id) {
    time_t now = time(NULL);
    /* 已有同凭证条目 → 刷新 */
    for (int i = 0; i < g_auth_cache_n; i++) {
        if (strcmp(g_auth_cache[i].product_key, pk) == 0 &&
            strcmp(g_auth_cache[i].secret, secret) == 0) {
            g_auth_cache[i].cached_at = now;
            if (device_id[0])
                snprintf(g_auth_cache[i].device_id,
                         sizeof(g_auth_cache[i].device_id), "%s", device_id);
            return;
        }
    }
    /* 满了淘汰最旧 (memmove 前移, 表尾腾出空槽) */
    int victim = 0;
    if (g_auth_cache_n >= AUTH_CACHE_CAP) {
        for (int i = 1; i < g_auth_cache_n; i++)
            if (g_auth_cache[i].cached_at < g_auth_cache[victim].cached_at)
                victim = i;
        memmove(&g_auth_cache[victim], &g_auth_cache[victim + 1],
                sizeof(auth_cache_ent_t) * (size_t)(g_auth_cache_n - victim - 1));
        g_auth_cache_n--;
    }
    auth_cache_ent_t *e = &g_auth_cache[g_auth_cache_n++];
    snprintf(e->product_key, sizeof(e->product_key), "%s", pk);
    snprintf(e->secret,     sizeof(e->secret),     "%s", secret);
    snprintf(e->device_id,  sizeof(e->device_id),  "%s", device_id);
    e->cached_at = now;
}

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

mqtt_connection_t *mqtt_broker_find_conn_by_device(const char *device_id) {
    if (!device_id) return NULL;
    pthread_mutex_lock(&g_conn_mutex);
    for (int i = 0; i < g_conn_count; i++) {
        mqtt_connection_t *conn = g_conns[i];
        if (conn && conn->connected && conn->device_id[0] &&
            strcmp(conn->device_id, device_id) == 0) {
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
    /* 队列 key 用 device_id：handler_command 入队时用的就是 device_id */
    offline_cmd_t *list = cmd_mgr_dequeue_all(conn->device_id);
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
    int db_down = 0;
    char auth_device_id[MQTT_ID_MAX] = {0};
    {
        db_conn_t *db = db_pool_get();
        if (!db) db_down = 1;
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
        if (db_down) {
            /* DB 不可用: 查最近成功认证缓存 (M3), TTL 内放行 */
            const char *cached = auth_cache_lookup(username, password);
            if (cached) {
                ok = 1;
                snprintf(auth_device_id, sizeof(auth_device_id), "%s", cached);
                LOG_WARN("CONNECT auth via cache (database down): user=%s dev=%s",
                         username, auth_device_id);
            } else {
                /* DB 不可用与凭证错误分开回码：0x03 server unavailable。
                 * 否则设备端把平台故障当成密钥错误，盲目重置密钥越描越黑。 */
                LOG_ERROR("CONNECT auth unavailable: database down, no cache (user=%s)", username);
                goto refused_db;
            }
        } else {
            LOG_ERROR("auth failed for user=%s", username);
            goto refused;
        }
    } else {
        /* 真实 DB 认证成功 → 刷新缓存, 供 DB 故障窗口内重连兜底 */
        auth_cache_store(username, password, auth_device_id);
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
    return;

refused_db:
    {
        mqtt_packet_t ack = {0};
        ack.fix_header.type  = MQTT_CONNACK;
        ack.fix_header.flags = 0;
        ack.fix_header.remain_len = 2;
        ack.vh.connack.session_present = 0;
        ack.vh.connack.return_code   = MQTT_CONNACK_REFUSED_SERVER;
        send_packet(conn->fd, &ack);
    }
}

/* ------------------------------------------------------------------ */
/* SUBSCRIBE                                                            */
/* ------------------------------------------------------------------ */
static void handle_subscribe(mqtt_connection_t *conn, mqtt_packet_t *pkt) {
    uint8_t *data = pkt->payload; uint32_t len = pkt->payload_len;
    if (len < 2) return;
    /* 未完成 CONNECT 认证的连接不允许订阅 (MQTT 协议顺序要求) */
    if (!conn->authenticated) {
        LOG_WARN("SUBSCRIBE before CONNECT (fd=%d) — rejected", conn->fd);
        return;
    }
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

        /* ACL (L3): cmd/ 指令 topic 只能订阅自己的
         * (cmd/<device_id>/exec 第 2 段 == 本连接认证身份),
         * 否则已认证设备可监听其他设备的指令。 */
        if (strncmp(tmp, "cmd/", 4) == 0) {
            const char *own = conn->device_id[0] ? conn->device_id : conn->client_id;
            const char *rest = tmp + 4;
            const char *slash = strchr(rest, '/');
            size_t id_len = slash ? (size_t)(slash - rest) : strlen(rest);
            if (own[0] == '\0' || strlen(own) != id_len ||
                strncmp(rest, own, id_len) != 0) {
                LOG_WARN("SUB denied by cmd ACL: '%s' (cid=%s dev=%s)",
                         tmp, conn->client_id, conn->device_id);
                return_codes[rc_count++] = 0x80;
                continue;
            }
        }

        /* 存储 subscription（同连接同 topic 去重：覆盖 qos 而非追加，
         * 否则设备端重复 SUBSCRIBE 会耗尽 MAX_SUBS_PER_CONN 槽位） */
        {
            int dup = 0;
            for (int di = 0; di < conn->sub_count; di++) {
                if (strcmp(conn->subs[di].topic, tmp) == 0) {
                    conn->subs[di].qos = qos;
                    dup = 1;
                    break;
                }
            }
            if (dup) {
                return_codes[rc_count++] = qos;
                LOG_DEBUG("SUB duplicate topic='%s', qos updated", tmp);
                continue;
            }
        }
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

/* 把一个数据点提交到后台写库线程池 (与 MQTT datapoints 上报同一条
 * publish_worker 链路: 注册校验/物模型/告警评估/分表入库/latest 更新)。
 * 返回 0 已提交; -1 参数非法或队列满被丢弃。
 * 供 MQTT PUBLISH 与 OneNET REST (handler_onenet) 两条链路共用。 */
static void publish_worker(void *arg);
int mqtt_broker_submit_datapoint(const char *product_key, const char *device_id,
                                 const char *metric, double value, uint64_t ts) {
    if (!g_thread_pool || !product_key || !product_key[0] ||
        !device_id || !device_id[0] || !metric || !metric[0]) return -1;

    publish_task_t *task = (publish_task_t *)calloc(1, sizeof(publish_task_t));
    if (!task) return -1;
    strncpy(task->product_key, product_key, sizeof(task->product_key) - 1);
    strncpy(task->device_id, device_id, sizeof(task->device_id) - 1);
    strncpy(task->metric, metric, sizeof(task->metric) - 1);
    task->value = value;
    task->ts = ts ? ts : (uint64_t)time(NULL);

    if (thread_pool_submit(g_thread_pool, publish_worker, task) != 0) {
        LOG_WARN("thread_pool_submit failed, drop datapoint (%s %s)", device_id, metric);
        free(task);
        return -1;
    }
    return 0;
}

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

    /* 设备必须先注册（客户端 UI / REST），未注册设备的上报直接拒绝。
     * （旧逻辑是 INSERT ... ON DUPLICATE 自动注册，任意 device_id 都能
     *  自我说成在线设备，数据面不可信 —— 已移除。） */
    {
        char chk_sql[256];
        snprintf(chk_sql, sizeof(chk_sql),
            "SELECT 1 FROM devices WHERE device_id='%s' AND product_key='%s' LIMIT 1",
            esc_id, esc_pk);
        MYSQL_RES *chk_res = (MYSQL_RES *)db_pool_query(db, chk_sql);
        int exists = 0;
        if (chk_res) {
            exists = mysql_fetch_row(chk_res) != NULL;
            db_pool_free_result(chk_res);
        }
        if (!exists) {
            LOG_WARN("publish dropped: device '%s' (pk=%s) not registered — "
                     "register via client/REST first", task->device_id, task->product_key);
            db_pool_put(db);
            free(task);
            return;
        }
    }

    /* 物模型白名单（轻量）: 产品未定义任何属性 = 自由模式放行；
     * 定义后，白名单外 identifier / bool 类型值不符 → 拒绝。
     * 复用已持有的 db 连接, 不嵌套占用池连接 */
    {
        tm_check_t tm = thing_model_check_with_conn(db, task->product_key,
                                                    task->metric, task->value);
        if (tm != TM_OK && tm != TM_FREE) {
            LOG_WARN("publish dropped: metric '%s' %s (pk=%s dev=%s)",
                     task->metric,
                     tm == TM_UNKNOWN ? "not in product model (10411)" : "type mismatch",
                     task->product_key, task->device_id);
            db_pool_put(db);
            free(task);
            return;
        }
    }

    /* 告警评估（复用当前连接，避免多连接死锁） */
    alarm_evaluate_with_conn(db, task->device_id, task->metric, task->value);

    /* 写入 data_reports_YYYYMM — 表名由数据点 ts 推导 (KNOWN_ISSUES L7:
     * 原按墙钟选表而查询按 ts 路由, 补报历史 ts 会写进当前月表但按 ts
     * 查不到; 且 shard_router 用 UTC, 墙钟 localtime 在月初还差 8 小时) */
    char tbl[64];
    shard_router_table_by_time((time_t)task->ts, tbl, sizeof(tbl));

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
        } else {
            /* 上报追踪: 计数 + 最近上报时间 (此前 report_count 恒 0) */
            char *cnt = (char *)malloc(256);
            if (cnt) {
                snprintf(cnt, 256,
                    "UPDATE devices SET report_count=report_count+1, "
                    "last_report_at=NOW() WHERE device_id='%s'", esc_id);
                db_pool_exec(db, cnt);
                free(cnt);
            }
        }
        free(ins);
    }

    /* 更新 device_latest_data */
    char *upsert = (char *)malloc(768);
    if (upsert) {
        snprintf(upsert, 768,
            "INSERT INTO device_latest_data (device_id, metric, value, ts, updated_at) "
            "VALUES ('%s','%s',%.2f,%llu,NOW()) "
            "ON DUPLICATE KEY UPDATE "
            "value=IF(ts<VALUES(ts),VALUES(value),value), "
            "ts=GREATEST(ts,VALUES(ts)), updated_at=NOW()",
            esc_id, esc_metric, task->value,
            (unsigned long long)task->ts);
        db_pool_exec(db, upsert);
        free(upsert);
    }

    db_pool_put(db);
    free(task);
}

/* ------------------------------------------------------------------ */
/* OneNET 物模型兼容 (路线A): $sys/{pid}/{did}/thing/property/post      */
/* ------------------------------------------------------------------ */

/* 单个参数值 → 数值。支持三种形态：
 *   number / bool / "on"-"off"-"true"-"false" 字符串（扁平写法）
 *   {"value": <以上任意>}（OneNET 嵌套写法，递归取 value 成员）
 * 可转换返回 1 并写 *out；不可转换返回 0。 */
static int onenet_param_value(const cJSON *item, double *out) {
    if (cJSON_IsNumber(item)) {
        *out = item->valuedouble;
        return 1;
    }
    if (cJSON_IsBool(item)) {
        *out = cJSON_IsTrue(item) ? 1.0 : 0.0;
        return 1;
    }
    if (cJSON_IsString(item) && item->valuestring) {
        if (strcmp(item->valuestring, "on") == 0 ||
            strcmp(item->valuestring, "true") == 0) {
            *out = 1.0;
            return 1;
        }
        if (strcmp(item->valuestring, "off") == 0 ||
            strcmp(item->valuestring, "false") == 0) {
            *out = 0.0;
            return 1;
        }
        return 0;
    }
    if (cJSON_IsObject(item)) {
        const cJSON *v = cJSON_GetObjectItem(item, "value");
        return v ? onenet_param_value(v, out) : 0;
    }
    return 0;
}

/* 解析 $sys/{pid}/{did}/thing/property/post 的 OneJSON
 * payload {"id":"..","params":{..}}，把 params 的每个键值转成 datapoint
 * 提交线程池入库（与 devices/{id}/data 同一条链路）。
 * device_id 取自 topic 第 3 段（OneNET 语义里与 client_id 一致），
 * product_key 用连接认证时的 username。 */
static void onenet_property_ingest(mqtt_connection_t *conn,
                                   const char *topic, const char *json_str) {
    char dev_id[MQTT_ID_MAX];
    const char *p = topic + 5;                  /* 跳过 "$sys/" */
    const char *s1 = strchr(p, '/');
    const char *s2 = s1 ? strchr(s1 + 1, '/') : NULL;

    if (!s1 || !s2 || s2 - s1 - 1 <= 0 ||
        (size_t)(s2 - s1 - 1) >= sizeof(dev_id)) {
        LOG_WARN("ONENET property post: bad topic '%s'", topic);
        return;
    }
    memcpy(dev_id, s1 + 1, (size_t)(s2 - s1 - 1));
    dev_id[s2 - s1 - 1] = '\0';

    /* 身份绑定 (M9): topic 中的 did 必须等于本连接认证身份 */
    if (!conn->device_id[0] || strcmp(dev_id, conn->device_id) != 0) {
        LOG_WARN("ONENET property post denied: topic dev=%s != authenticated %s (cid=%s)",
                 dev_id, conn->device_id[0] ? conn->device_id : "(none)", conn->client_id);
        return;
    }

    cJSON *root = cJSON_Parse(json_str);
    const cJSON *params = root ? cJSON_GetObjectItem(root, "params") : NULL;
    if (!cJSON_IsObject(params)) {
        LOG_WARN("ONENET property post: missing params object (dev=%s)", dev_id);
        cJSON_Delete(root);
        return;
    }

    int ingested = 0;
    cJSON *item;
    cJSON_ArrayForEach(item, params) {
        double val;

        if (!item->string || !item->string[0]) continue;
        if (!onenet_param_value(item, &val)) {
            LOG_DEBUG("ONENET param '%s' non-numeric, skip", item->string);
            continue;
        }

        if (mqtt_broker_submit_datapoint(conn->product_key, dev_id,
                                         item->string, val,
                                         (uint64_t)time(NULL)) == 0) {
            ingested++;
        }
    }
    cJSON_Delete(root);
    LOG_INFO("ONENET property post dev=%s ingested %d datapoint(s)", dev_id, ingested);
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

    /* ACL (L3): 指令只由服务端下发 (mqtt_broker_send_cmd 直达目标连接,
     * 不经过本函数), 设备侧 PUBLISH 到 cmd 指令 topic 一律拒绝 —
     * 否则已认证设备可伪造发给其他设备的指令。 */
    if (strncmp(topic, "cmd/", 4) == 0) {
        LOG_WARN("PUBLISH denied (cmd topic from device): '%s' cid=%s",
                 topic, conn->client_id);
        return;
    }

    LOG_INFO("PUBLISH from fd=%d topic=%s payload_len=%u",
             conn->fd, topic, len - 2 - tlen);

    /* QoS>0 的 PUBLISH 在 topic 之后还有一个 2 字节 packet_id，
     * 应用层 payload 必须跳过它，否则 JSON 从 packet_id 开始解析必然失败。 */
    uint8_t pub_qos = (pkt->fix_header.flags >> 1) & 0x03;
    uint32_t app_off = 2 + tlen + (pub_qos > 0 ? 2 : 0);

    /* 主线程: 解析 JSON 并提交后台任务 */
    {
        uint32_t payload_off = app_off;
        uint32_t payload_len = len - payload_off;
        if (payload_len > 0) {
            char *json_str = (char *)malloc(payload_len + 1);
            if (json_str) {
                memcpy(json_str, data + payload_off, payload_len);
                json_str[payload_len] = '\0';

                /* OneNET 物模型兼容: $sys/{pid}/{did}/thing/property/post
                 * 走 params 逐键值入库；其余按 devices/{id}/data 处理。
                 * json_str 统一由块尾的 free 释放，分支内不得重复释放。 */
                if (strncmp(topic, "$sys/", 5) == 0 &&
                    strstr(topic, "/thing/property/post") != NULL) {
                    onenet_property_ingest(conn, topic, json_str);
                } else {
                cJSON *root = cJSON_Parse(json_str);
                if (root) {
                    const cJSON *id = cJSON_GetObjectItem(root, "device_id");
                    const cJSON *dp = cJSON_GetObjectItem(root, "datapoints");
                    /* 身份绑定 (M9): 上报 device_id 必须等于本连接认证身份
                     * (认证 = product_key + 该设备独立 secret), 防止同产品
                     * 设备互相冒充; conn->device_id 由认证 SQL 从 devices
                     * 表取回, 恒非空, 空值防御一并拒绝 */
                    if (!conn->device_id[0] ||
                        (id && id->valuestring && strcmp(id->valuestring, conn->device_id) != 0)) {
                        LOG_WARN("PUBLISH dropped: identity mismatch (payload dev=%s, auth dev=%s cid=%s)",
                                 id && id->valuestring ? id->valuestring : "(none)",
                                 conn->device_id[0] ? conn->device_id : "(none)",
                                 conn->client_id);
                    } else if (id && id->valuestring && dp && cJSON_IsArray(dp)) {
                        cJSON *item;
                        cJSON_ArrayForEach(item, dp) {
                            const cJSON *metric = cJSON_GetObjectItem(item, "metric");
                            const cJSON *value = cJSON_GetObjectItem(item, "value");
                            const cJSON *ts   = cJSON_GetObjectItem(item, "ts");
                            if (metric && metric->valuestring && value) {
                                double val = value->valuedouble;
                                uint64_t ts_val = ts ? (uint64_t)ts->valuedouble : (uint64_t)time(NULL);

                                /* 把 DB+告警 任务提交到线程池 */
                                mqtt_broker_submit_datapoint(conn->product_key,
                                                             id->valuestring,
                                                             metric->valuestring,
                                                             val, ts_val);
                            }
                        }
                    } else {
                        LOG_WARN("PUBLISH payload missing device_id or datapoints");
                    }
                    cJSON_Delete(root);
                } else {
                    LOG_WARN("PUBLISH payload JSON parse failed");
                }
                } /* else: 非 OneNET topic 的 datapoints 路径 */
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
                /* 向下游转发时统一以 QoS0 重新组包：
                 * 原报文若为 QoS1/2，topic 之后有 2 字节 packet_id，
                 * 直接透传会让订阅者把 packet_id 当成数据的一部分。 */
                {
                    uint32_t app_len = len - app_off;            /* 应用层 payload 长度 */
                    uint32_t fwd_len = 2 + tlen + app_len;
                    uint8_t *fwd_raw = malloc(fwd_len);
                    if (!fwd_raw) continue;

                    fwd_raw[0] = data[0];                        /* topic 长度(原样) */
                    fwd_raw[1] = data[1];
                    memcpy(fwd_raw + 2, data + 2, tlen);         /* topic */
                    memcpy(fwd_raw + 2 + tlen, data + app_off, app_len);  /* 跳过 packet_id */

                    mqtt_packet_t fwd = {0};
                    fwd.fix_header.type  = MQTT_PUBLISH;
                    fwd.fix_header.flags = 0;                    /* QoS 0, no retain */
                    fwd.payload     = fwd_raw;
                    fwd.payload_len = fwd_len;

                    send_packet(target->fd, &fwd);
                    free(fwd_raw);
                }
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
