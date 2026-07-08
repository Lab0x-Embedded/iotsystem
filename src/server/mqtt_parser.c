/**
 * @file mqtt_parser.c
 *
 * MQTT 3.1.1 报文 字节流 → mqtt_packet_t 状态机实现.
 *
 * 状态机:
 *   PARSE_STATE_FIX_HEADER   读 1 字节 type|flags
 *   PARSE_STATE_REMAIN_LEN   读 1~4 字节变长整数
 *   PARSE_STATE_VARIABLE_HEADER  按 type 读对应长度
 *   PARSE_STATE_PAYLOAD      读 remain_len - var_header_len 字节
 *   PARSE_STATE_COMPLETE     完整报文, 可 take
 *
 * 限制 (Phase 2 最小集):
 *   - 只完整解析 CONNECT / SUBSCRIBE / PINGREQ / DISCONNECT / PUBLISH
 *   - 其他类型仅解析固定头 + 剩余长度, 载荷指针留空
 *   - 最大报文长度 256 KB
 */

#include "mqtt_parser.h"
#include "common/log.h"

#include <stdlib.h>
#include <string.h>
#include <errno.h>

#define MQTT_MAX_PACKET_SIZE (256 * 1024)

/* ------------------------------------------------------------------ */
/* 状态                                                                */
/* ------------------------------------------------------------------ */
typedef enum {
    PARSE_STATE_FIX_HEADER = 0,
    PARSE_STATE_REMAIN_LEN,
    PARSE_STATE_PAYLOAD,
    PARSE_STATE_COMPLETE,
} parse_state_t;

/* ------------------------------------------------------------------ */
/* 解析器对象                                                          */
/* ------------------------------------------------------------------ */
struct mqtt_parser {
    parse_state_t  state;

    /* 正在解析的报文 */
    mqtt_packet_t  current;

    /* REMAIN_LEN 状态暂存 */
    uint8_t  remain_bytes[4];
    int      remain_idx;
    uint32_t remain_acc;

    /* VAR_HEADER 阶段偏移 (我们把定长 var_header 当作 payload 的一部分读) */
    uint32_t var_header_len;

    /* 载荷写入偏移 */
    uint32_t payload_off;
};

/* ------------------------------------------------------------------ */
/* 辅助: 读 2 字节 MSB/LSB 整数                                        */
/* ------------------------------------------------------------------ */
static uint16_t read_u16(const uint8_t *p) {
    return (uint16_t)((p[0] << 8) | p[1]);
}

/* ------------------------------------------------------------------ */
/* 创建 / 销毁                                                         */
/* ------------------------------------------------------------------ */
mqtt_parser_t *mqtt_parser_create(void) {
    mqtt_parser_t *p = calloc(1, sizeof(*p));
    if (!p) return NULL;
    p->state = PARSE_STATE_FIX_HEADER;
    return p;
}

void mqtt_parser_destroy(mqtt_parser_t *p) {
    if (!p) return;
    mqtt_packet_unref(&p->current);
    free(p);
}

/* ------------------------------------------------------------------ */
/* 状态机: 定长头 → 剩余长度 → 载荷 → 完成                            */
/* ------------------------------------------------------------------ */
static void reset_state(mqtt_parser_t *p) {
    /* 注意: current.payload 由调用方 unrec 后才调用 reset */
    p->state = PARSE_STATE_FIX_HEADER;
    p->remain_idx = 0;
    p->remain_acc = 0;
    p->var_header_len = 0;
    p->payload_off = 0;
    memset(&p->current.fix_header, 0, sizeof(p->current.fix_header));
}

/* 是否是可完整解析的类型 (Phase 2 最小集) */
static int full_parse_type(uint8_t type) {
    return type == MQTT_CONNECT
        || type == MQTT_SUBSCRIBE
        || type == MQTT_UNSUBSCRIBE
        || type == MQTT_PINGREQ
        || type == MQTT_DISCONNECT
        || type == MQTT_PUBLISH;
}

/* ------------------------------------------------------------------ */
/* 核心: 喂入一个字节                                                  */
/* ------------------------------------------------------------------ */
int mqtt_parser_feed(mqtt_parser_t *p, uint8_t byte) {
    switch (p->state) {

    case PARSE_STATE_FIX_HEADER: {
        p->current.fix_header.type  = (byte >> 4) & 0x0F;
        p->current.fix_header.flags = byte & 0x0F;
        p->state = PARSE_STATE_REMAIN_LEN;
        p->remain_idx = 0;
        p->remain_acc = 0;
        return 0;
    }

    case PARSE_STATE_REMAIN_LEN: {
        p->remain_bytes[p->remain_idx++] = byte;
        p->remain_acc += (uint32_t)(byte & 0x7F) << (7 * (p->remain_idx - 1));

        if ((byte & 0x80) == 0 || p->remain_idx >= 4) {
            p->current.fix_header.remain_len = p->remain_acc;

            if (p->remain_acc > MQTT_MAX_PACKET_SIZE) {
                LOG_ERROR("remain_len %u > MAX %d, drop",
                          p->remain_acc, MQTT_MAX_PACKET_SIZE);
                reset_state(p);
                return 0;
            }

            if (p->remain_acc == 0) {
                /* PINGREQ / DISCONNECT 这种就是 0 长度 payload */
                p->state = PARSE_STATE_COMPLETE;
                return 1;
            }

            if (full_parse_type(p->current.fix_header.type)) {
                /* 分配 payload buffer */
                p->current.payload = malloc(p->remain_acc);
                if (!p->current.payload) {
                    LOG_ERROR("OOM: malloc %u", p->remain_acc);
                    reset_state(p);
                    return 0;
                }
                p->current.payload_len = p->remain_acc;
                p->payload_off = 0;
                p->state = PARSE_STATE_PAYLOAD;
            } else {
                /* 不可解析类型 → 直接就绪, 调用方自己处理 payload */
                p->current.payload     = NULL;
                p->current.payload_len = 0;
                p->state = PARSE_STATE_COMPLETE;
                return 1;
            }
        }
        return 0;
    }

    case PARSE_STATE_PAYLOAD: {
        p->current.payload[p->payload_off++] = byte;
        if (p->payload_off >= p->current.payload_len) {
            p->state = PARSE_STATE_COMPLETE;
            return 1;
        }
        return 0;
    }

    case PARSE_STATE_COMPLETE:
        /* 应该先 take 再 feed; 这里防御性重置 */
        reset_state(p);
        return mqtt_parser_feed(p, byte);
    }
    return 0;
}

/* ------------------------------------------------------------------ */
/* 业务层: 解码已经收齐的 CONNECT 载荷                                */
/* ------------------------------------------------------------------ */
static int parse_connect_payload(mqtt_parser_t *p) {
    uint8_t *data = p->current.payload;
    uint32_t len  = p->current.payload_len;
    uint32_t off  = 0;

    if (len < 12) { LOG_ERROR("CONNECT payload too short %u", len); return -1; }

    /* Protocol Name (2 len + 4 name) */
    uint16_t pn_len = read_u16(data + off); off += 2;
    if (pn_len != 4 || off + pn_len > len) { LOG_ERROR("CONNECT bad pname"); return -1; }
    memcpy(p->current.vh.connect.protocol_name, data + off, pn_len);
    p->current.vh.connect.protocol_name[pn_len] = '\0';
    off += pn_len;

    /* Protocol Level */
    if (off >= len) return -1;
    p->current.vh.connect.protocol_level = data[off++];

    /* Connect Flags */
    if (off >= len) return -1;
    p->current.vh.connect.connect_flags = data[off++];

    /* Keepalive */
    if (off + 2 > len) return -1;
    p->current.vh.connect.keepalive = read_u16(data + off); off += 2;

    /* Client ID (2 len + uuid style) */
    if (off + 2 > len) return -1;
    uint16_t cid_len = read_u16(data + off); off += 2;
    if (off + cid_len > len) return -1;
    /* client_id 填充到 session, 这里暂存进 rx_buf 最大值 */
    /* (Phase 2 minimal: 接收后交给 broker 处理) */
    return 0;
}

/* ------------------------------------------------------------------ */
/* 取走报文对象                                                        */
/* ------------------------------------------------------------------ */
mqtt_packet_t *mqtt_parser_take(mqtt_parser_t *p) {
    if (p->state != PARSE_STATE_COMPLETE) return NULL;

    /* clone 出一份给调用方 */
    mqtt_packet_t *out = mqtt_packet_alloc();
    if (!out) return NULL;
    out->fix_header  = p->current.fix_header;
    out->payload     = p->current.payload;
    out->payload_len = p->current.payload_len;
    out->ref_count   = 1;

    /* 解码 CONNECT 语义 (先解析, 再赋值 vh, 确保 parse_connect_payload 写入 p->current.vh) */
    if (out->fix_header.type == MQTT_CONNECT) {
        if (parse_connect_payload(p) != 0) {
            LOG_ERROR("parse_connect_payload failed");
        }
    }
    out->vh = p->current.vh;

    /* 复位 current */
    memset(&p->current, 0, sizeof(p->current));
    reset_state(p);
    return out;
}

/* ------------------------------------------------------------------ */
/* mqtt_packet_t 对象管理                                              */
/* ------------------------------------------------------------------ */
mqtt_packet_t *mqtt_packet_alloc(void) {
    mqtt_packet_t *p = calloc(1, sizeof(*p));
    if (p) p->ref_count = 1;
    return p;
}

void mqtt_packet_ref(mqtt_packet_t *pkt) {
    if (pkt) pkt->ref_count++;
}

void mqtt_packet_unref(mqtt_packet_t *pkt) {
    if (!pkt) return;
    if (--pkt->ref_count == 0) {
        free(pkt->payload);
        free(pkt);
    }
}

/* ------------------------------------------------------------------ */
/* 辅助: 从 payload 里解码 Uint16 (读 packet_id 等)                    */
/* ------------------------------------------------------------------ */
uint16_t mqtt_read_u16(const uint8_t *p) {
    return (uint16_t)((p[0] << 8) | p[1]);
}

/* ------------------------------------------------------------------ */
/* 辅助: 从 payload 里解码 UTF-8 字符串                                */
/* ------------------------------------------------------------------ */
uint32_t mqtt_read_string(const uint8_t *p, uint32_t len, char *out, size_t cap) {
    if (len < 2) return 0;
    uint16_t slen = mqtt_read_u16(p);
    if (slen + 2 > len) return 0;
    if (slen + 1 > cap) slen = (uint16_t)(cap - 1);
    memcpy(out, p + 2, slen);
    out[slen] = '\0';
    return slen;
}
