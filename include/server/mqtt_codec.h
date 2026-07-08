/**
 * @file mqtt_codec.h
 *
 * mqtt_packet_t → 字节流编码器.
 *
 * P2 最小集: 只编码 CONNACK / SUBACK / PINGRESP / PUBACK / DISCONNECT 这些
 * server → client 方向报文.
 */

#ifndef E2_MQTT_CODEC_H
#define E2_MQTT_CODEC_H

#include "mqtt_base.h"

typedef struct {
    uint8_t *data;
    uint32_t len;
    uint32_t cap;
} mqtt_buf_t;

/** 编码后返回 buf; 调用者负责 free(buf.data). */
mqtt_buf_t mqtt_encode(const mqtt_packet_t *pkt);

/** 直接写入提供的 buf, 返回写入字节数; buf 不足返回 0. */
uint32_t  mqtt_encode_into(const mqtt_packet_t *pkt, uint8_t *buf, uint32_t cap);

#endif /* E2_MQTT_CODEC_H */
