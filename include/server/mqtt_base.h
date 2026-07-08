/**
 * @file mqtt_base.h  — 基础类型, 被 mqtt_parser / mqtt_codec 使用
 */
#ifndef E2_MQTT_BASE_H
#define E2_MQTT_BASE_H

#include <stdint.h>
#include <stddef.h>

typedef enum {
    MQTT_RESERVED     = 0,
    MQTT_CONNECT      = 1,
    MQTT_CONNACK      = 2,
    MQTT_PUBLISH      = 3,
    MQTT_PUBACK       = 4,
    MQTT_PUBREC       = 5,
    MQTT_PUBREL       = 6,
    MQTT_PUBCOMP      = 7,
    MQTT_SUBSCRIBE    = 8,
    MQTT_SUBACK       = 9,
    MQTT_UNSUBSCRIBE  = 10,
    MQTT_UNSUBACK     = 11,
    MQTT_PINGREQ      = 12,
    MQTT_PINGRESP     = 13,
    MQTT_DISCONNECT   = 14,
} mqtt_msg_type_t;

typedef enum {
    MQTT_QOS_AT_MOST_ONCE  = 0,
    MQTT_QOS_AT_LEAST_ONCE = 1,
    MQTT_QOS_EXACTLY_ONCE  = 2,
} mqtt_qos_t;

typedef enum {
    MQTT_CONNACK_ACCEPTED              = 0,
    MQTT_CONNACK_REFUSED_PROTOCOL      = 1,
    MQTT_CONNACK_REFUSED_IDENTIFIER    = 2,
    MQTT_CONNACK_REFUSED_SERVER        = 3,
    MQTT_CONNACK_REFUSED_BAD_CREDS     = 4,
    MQTT_CONNACK_REFUSED_NOT_AUTH      = 5,
} mqtt_connack_code_t;

typedef struct {
    uint8_t  type;
    uint8_t  flags;
    uint32_t remain_len;
} mqtt_fix_header_t;

typedef struct {
    char     protocol_name[8];
    uint8_t  protocol_level;
    uint8_t  connect_flags;
    uint16_t keepalive;
} mqtt_connect_vh_t;

typedef struct {
    uint8_t  session_present;
    uint8_t  return_code;
} mqtt_connack_vh_t;

typedef struct {
    uint16_t packet_id;
} mqtt_id_vh_t;

typedef struct mqtt_packet {
    mqtt_fix_header_t fix_header;
    union {
        mqtt_connect_vh_t  connect;
        mqtt_connack_vh_t  connack;
        mqtt_id_vh_t       id;
    } vh;
    uint8_t  *payload;
    uint32_t  payload_len;
    uint16_t  ref_count;
} mqtt_packet_t;

mqtt_packet_t *mqtt_packet_alloc(void);
void           mqtt_packet_ref(mqtt_packet_t *pkt);
void           mqtt_packet_unref(mqtt_packet_t *pkt);

uint16_t mqtt_read_u16(const uint8_t *p);

#endif /* E2_MQTT_BASE_H */
