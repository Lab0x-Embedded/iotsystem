/**
 * @file mqtt_codec.c
 */
#include "mqtt_codec.h"
#include "mqtt_parser.h"     /* 复用 mqtt_packet_t */
#include "common/log.h"

#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* 写 1 字节                                                          */
/* ------------------------------------------------------------------ */
static uint32_t write_byte(uint8_t *p, uint8_t v) {
    p[0] = v;
    return 1;
}

/* ------------------------------------------------------------------ */
/* 写 Uint16 (MSB/LSB)                                                 */
/* ------------------------------------------------------------------ */
static uint32_t write_u16(uint8_t *p, uint16_t v) {
    p[0] = (v >> 8) & 0xFF;
    p[1] = v & 0xFF;
    return 2;
}

/* ------------------------------------------------------------------ */
/* 写变长整数                                                          */
/* ------------------------------------------------------------------ */
static uint32_t write_remain_len(uint8_t *p, uint32_t v) {
    uint32_t n = 0;
    do {
        uint8_t b = v % 128;
        v /= 128;
        if (v > 0) b |= 0x80;
        p[n++] = b;
    } while (v > 0);
    return n;
}

#if 0  /* 备用: 写 MQTT 字符串 (2 字节长度 + 数据) */
static uint32_t write_string(uint8_t *p, const char *s) {
    uint16_t slen = (uint16_t)strlen(s);
    write_u16(p, slen);
    memcpy(p + 2, s, slen);
    return 2 + slen;
}
#endif

/* ------------------------------------------------------------------ */
/* 估算一报文编码后多长                                                */
/* ------------------------------------------------------------------ */
static uint32_t compute_length(const mqtt_packet_t *pkt) {
    uint32_t vh = 0, plen = pkt->payload_len;
    switch (pkt->fix_header.type) {
    case MQTT_CONNACK:    vh = 2; break;
    case MQTT_SUBACK:     vh = 2 + plen; break;  /* 2 字节 packet_id + return codes */
    case MQTT_PUBACK:
    case MQTT_UNSUBACK:   vh = 2; break;
    case MQTT_PINGRESP:   vh = 0; break;
    case MQTT_DISCONNECT: vh = 0; break;
    case MQTT_PUBLISH:
        /* 2 字节 topic + (qos>0 ? 2 字节 packet_id : 0) + payload */
        if (plen >= 2) {
            uint16_t tlen = (pkt->payload[0] << 8) | pkt->payload[1];
            vh = 2 + tlen + (((pkt->fix_header.flags >> 1) & 0x03) > 0 ? 2 : 0);
        }
        break;
    default:
        vh = 0;
        break;
    }
    return vh + plen;
}

mqtt_buf_t mqtt_encode(const mqtt_packet_t *pkt) {
    uint32_t remain = compute_length(pkt);
    uint32_t nlen  = 1;  /* 变长整数至少 1 字节 */
    for (uint32_t t = remain; t >= 128; t /= 128) nlen++;

    uint32_t total = 1 + nlen + remain;  /* fix_header_byte + remain_len + remain */
    uint8_t *data = malloc(total);
    if (!data) return (mqtt_buf_t){NULL, 0, 0};

    uint32_t off = 0;
    data[off++] = (pkt->fix_header.type << 4) | (pkt->fix_header.flags & 0x0F);
    off += write_remain_len(data + off, remain);

    switch (pkt->fix_header.type) {
    case MQTT_CONNACK:
        off += write_byte(data + off, pkt->vh.connack.session_present);
        off += write_byte(data + off, pkt->vh.connack.return_code);
        break;
    case MQTT_SUBACK:
    case MQTT_PUBACK:
    case MQTT_UNSUBACK:
        off += write_u16(data + off, pkt->vh.id.packet_id);
        /* SUBACK: return codes 在 payload 里 */
        if (pkt->fix_header.type == MQTT_SUBACK && pkt->payload_len > 0) {
            memcpy(data + off, pkt->payload, pkt->payload_len);
            off += pkt->payload_len;
        }
        break;
    case MQTT_PUBLISH:
        if (pkt->payload_len > 0 && pkt->payload_len >= 2) {
            uint16_t tlen = ((uint16_t)pkt->payload[0] << 8) | pkt->payload[1];
            uint16_t qos  = (pkt->fix_header.flags >> 1) & 0x03;
            uint32_t vh_bytes = 2 + tlen + (qos > 0 ? 2 : 0);
            if (vh_bytes > pkt->payload_len) vh_bytes = pkt->payload_len;
            memcpy(data + off, pkt->payload, vh_bytes);   off += vh_bytes;
            if (vh_bytes < pkt->payload_len) {
                memcpy(data + off, pkt->payload + vh_bytes, pkt->payload_len - vh_bytes);
                off += pkt->payload_len - vh_bytes;
            }
        }
        break;
    default:
        if (pkt->payload && pkt->payload_len > 0) {
            memcpy(data + off, pkt->payload, pkt->payload_len);
            off += pkt->payload_len;
        }
        break;
    }

    return (mqtt_buf_t){data, off, total};
}

uint32_t mqtt_encode_into(const mqtt_packet_t *pkt, uint8_t *buf, uint32_t cap) {
    mqtt_buf_t b = mqtt_encode(pkt);
    if (!b.data || b.len > cap) {
        free(b.data);
        return 0;
    }
    memcpy(buf, b.data, b.len);
    free(b.data);
    return b.len;
}
