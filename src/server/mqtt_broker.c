/**
 * @file mqtt_broker.c
 *
 * P2 最小实现:
 *   - CONNECT   → 解析 client_id / username / password 硬编码认证
 *   - SUBSCRIBE → 简单逐连接 token = 0 (订阅返回码)
 *   - PUBLISH   → 按 topic 分发给 SUB (仅 QoS 0)
 *   - PINGREQ   → PINGRESP
 *   - DISCONNECT → 标记连接关闭
 */
#include "mqtt_broker.h"
#include "mqtt_parser.h"
#include "mqtt_codec.h"

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
/* session 表                                                           */
/* ------------------------------------------------------------------ */
#define MAX_SESSIONS 128
static mqtt_session_t g_sessions[MAX_SESSIONS];

void mqtt_broker_init(void) {
    memset(g_sessions, 0, sizeof(g_sessions));
}

/* ------------------------------------------------------------------ */
/* 辅助: 逐连接读 mqtt_packet_t 的载荷 UTF-8 string                       */
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

/* ------------------------------------------------------------------ */
/* 发字节到 fd                                                          */
/* ------------------------------------------------------------------ */
static int send_bytes(int fd, const uint8_t *buf, uint32_t len) {
    while (len > 0) {
        ssize_t n = send(fd, buf, len, 0);
        if (n > 0) { buf += n; len -= (uint32_t)n; continue; }
        if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) continue;
        return -1;
    }
    return 0;
}

/* ------------------------------------------------------------------ */
/* CONNECT                                                              */
/* ------------------------------------------------------------------ */
static void handle_connect(mqtt_connection_t *conn, mqtt_packet_t *pkt) {
    if (!conn->connected) {
        /* 已经认证过 → CONNACK 已发, 重复的 CONNACK 按协议也发 */
    }
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

    /* 解析 USERNAME / PASSWORD (可选) */
    char username[65] = {0};
    char password[65] = {0};

    uint8_t cflags = pkt->vh.connect.connect_flags;

    if (cflags & 0x80) { /* username */
        if (off + 2 > remain) goto refused;
        off += read_str(pkt->payload + off, remain - off, username, sizeof(username));
    }
    if (cflags & 0x40) { /* password */
        if (off + 2 > remain) goto refused;
        off += read_str(pkt->payload + off, remain - off, password, sizeof(password));
    }

    LOG_INFO("CONNECT cid=%s user=%s secret=%.4s...", client_id, username, password);

    /* ---- 硬编码认证 ---- */
    int ok = (strcmp(username, AUTH_PRODUCT_KEY) == 0) &&
             (strcmp(password, AUTH_DEVICE_SECRET) == 0);
    if (!ok) {
        LOG_ERROR("auth failed for user=%s", username);
        goto refused;
    }

    /* 存储 session */
    (void)conn; (void)pkt;
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
        mqtt_buf_t b = mqtt_encode(&ack);
        if (b.data) {
            int r = send_bytes(conn->fd, b.data, b.len);
            free(b.data);
            if (r != 0) {
                LOG_ERROR("send_bytes CONNACK failed: %s", strerror(errno));
            }
        }
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
        mqtt_buf_t b = mqtt_encode(&ack);
        if (b.data) {
            send_bytes(conn->fd, b.data, b.len);
            free(b.data);
        }
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
        memcpy(tmp, topic, tlen < 64 ? tlen : 64);
        topic[tlen < 64 ? tlen : 64] = '\0';
        off += tlen + 1;
        /* 存储 subscription */
        mqtt_subscription_t *sub = &conn->subs[conn->sub_count++];
        strncpy(sub->topic, tmp, sizeof(sub->topic) - 1);
        sub->qos = qos;
        return_codes[rc_count++] = qos; /* 同意 */
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
        mqtt_buf_t b = mqtt_encode(&ack);
        if (b.data) { send_bytes(conn->fd, b.data, b.len); free(b.data); }
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
    mqtt_buf_t b = mqtt_encode(&ack);
    if (b.data) { send_bytes(conn->fd, b.data, b.len); free(b.data); }
}

/* ------------------------------------------------------------------ */
/* 路由分发                                                             */
/* ------------------------------------------------------------------ */
void mqtt_broker_dispatch(mqtt_connection_t *conn, mqtt_packet_t *pkt) {
    if (!conn || !pkt) return;
    switch (pkt->fix_header.type) {
    case MQTT_CONNECT:    handle_connect(conn, pkt); break;
    case MQTT_SUBSCRIBE:  handle_subscribe(conn, pkt); break;
    case MQTT_PINGREQ:    handle_ping(conn); break;
    case MQTT_DISCONNECT: conn->connected = 0; break;
    case MQTT_PUBLISH:    break;   /* TODO: P3+ */
    case MQTT_UNSUBSCRIBE:break;
    default:
        LOG_WARN("unsupported msg type %d", pkt->fix_header.type);
        break;
    }
    mqtt_packet_unref(pkt);
}
