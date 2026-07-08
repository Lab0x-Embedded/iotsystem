/**
 * @file mqtt_broker.c
 *
 * Phase 2 基础 + Phase 3 PUBLISH/UNSUBSCRIBE:
 *   - CONNECT    → 解析 client_id / username / password 硬编码认证
 *   - SUBSCRIBE  → 记录订阅 topic, 回复 SUBACK
 *   - UNSUBSCRIBE→ 移除订阅 topic, 回复 UNSUBACK
 *   - PUBLISH    → 按 topic 分发给所有匹配的在线订阅者 (QoS 0)
 *   - PINGREQ    → PINGRESP
 *   - DISCONNECT → 标记连接关闭
 */
#include "mqtt_broker.h"
#include "mqtt_parser.h"
#include "mqtt_codec.h"
#include "mqtt_topic.h"

#include "common/log.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <errno.h>

/* ------------------------------------------------------------------ */
/* 硬编码认证凭据                                                       */
/* ------------------------------------------------------------------ */
#define AUTH_PRODUCT_KEY   "pk_test"
#define AUTH_DEVICE_ID     "dev_001"
#define AUTH_DEVICE_SECRET "secret_001"

/* ------------------------------------------------------------------ */
/* 全局 session 表 / 在线连接表                                          */
/* ------------------------------------------------------------------ */
#define MAX_SESSIONS 128
#define MAX_CONNS   256

static mqtt_session_t    g_sessions[MAX_SESSIONS];
static mqtt_connection_t *g_conns[MAX_CONNS];
static int g_conn_count;

void mqtt_broker_init(void) {
    memset(g_sessions, 0, sizeof(g_sessions));
    memset(g_conns, 0, sizeof(g_conns));
    g_conn_count = 0;
}

void mqtt_broker_register(mqtt_connection_t *conn) {
    if (!conn) return;
    if (g_conn_count >= MAX_CONNS) {
        LOG_ERROR("connection table full, can't register fd=%d", conn->fd);
        return;
    }
    g_conns[g_conn_count++] = conn;
    LOG_INFO("register conn fd=%d (total=%d)", conn->fd, g_conn_count);
}

void mqtt_broker_unregister(mqtt_connection_t *conn) {
    if (!conn) return;
    for (int i = 0; i < g_conn_count; i++) {
        if (g_conns[i] == conn) {
            g_conns[i] = g_conns[--g_conn_count];
            LOG_INFO("unregister conn fd=%d (total=%d)", conn->fd, g_conn_count);
            return;
        }
    }
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
    while (len > 0) {
        ssize_t n = send(fd, buf, len, 0);
        if (n > 0) { buf += n; len -= (uint32_t)n; continue; }
        if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) continue;
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
/* CONNECT                                                              */
/* ------------------------------------------------------------------ */
static void handle_connect(mqtt_connection_t *conn, mqtt_packet_t *pkt) {
    /* 协议层检查 */
    if (pkt->vh.connect.protocol_level != 4) {
        LOG_ERROR("unsupported protocol level %d", pkt->vh.connect.protocol_level);
        goto refused;
    }
    if (memcmp(pkt->vh.connect.protocol_name, "MQTT", 4) != 0) {
        LOG_ERROR("bad protocol name: %.8s", pkt->vh.connect.protocol_name);
        goto refused;
    }

    /* 解析 CLIENT ID */
    uint32_t remain = pkt->payload_len, off = 0;
    if (remain < 2) goto refused;
    uint16_t cid_len = ((uint16_t)pkt->payload[0] << 8) | pkt->payload[1];
    if (cid_len + 2 > remain) goto refused;
    char client_id[65];
    memset(client_id, 0, sizeof(client_id));
    memcpy(client_id, pkt->payload + 2, cid_len);
    client_id[cid_len] = '\0';
    off = 2 + cid_len;

    /* 解析 USERNAME / PASSWORD */
    char username[65] = {0};
    char password[65] = {0};
    uint8_t cflags = pkt->vh.connect.connect_flags;

    if (cflags & 0x80) {
        if (off + 2 > remain) goto refused;
        off += read_str(pkt->payload + off, remain - off, username, sizeof(username));
    }
    if (cflags & 0x40) {
        if (off + 2 > remain) goto refused;
        off += read_str(pkt->payload + off, remain - off, password, sizeof(password));
    }

    LOG_INFO("CONNECT cid=%s user=%s secret=%.4s...", client_id, username, password);

    /* 硬编码认证 */
    int ok = (strcmp(username, AUTH_PRODUCT_KEY) == 0) &&
             (strcmp(password, AUTH_DEVICE_SECRET) == 0);
    if (!ok) {
        LOG_ERROR("auth failed for user=%s", username);
        goto refused;
    }

    /* 存储 session */
    conn->connected = 1;
    conn->authenticated = 1;
    strncpy(conn->client_id, client_id, sizeof(conn->client_id) - 1);

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
/* PUBLISH (QoS 0): 收 → 按 topic 分发给所有匹配的在线订阅者             */
/* ------------------------------------------------------------------ */
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

    /* 遍历所有在线连接, 逐 subscription 匹配 */
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

    /* TODO: P4 — 如果 client 有 pending subscriber, 继续 */
    /* (data 不在这里 free, pkt 的 unref 由 dispatch 负责) */
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
    case MQTT_DISCONNECT:  conn->connected = 0; break;
    default:
        LOG_WARN("unsupported msg type %d", pkt->fix_header.type);
        break;
    }
    mqtt_packet_unref(pkt);
}
