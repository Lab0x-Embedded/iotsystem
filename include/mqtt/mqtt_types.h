/**
 * @file mqtt_types.h  — high-level MQTT objects (session, connection)
 */
#ifndef E2_MQTT_TYPES_H
#define E2_MQTT_TYPES_H

#include "mqtt_base.h"
#include "connection.h"

typedef struct mqtt_parser mqtt_parser_t;

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_SUBS_PER_CONN 8

/* 设备标识最大长度(含 '\0')。原为 65，真实模组的 client_id / secret 常超过 64 字符，
 * 超长会被静默截断导致认证莫名失败，这里放宽到 128 字符。 */
#define MQTT_ID_MAX 129

typedef struct {
    char    topic[64];
    uint8_t qos;
} mqtt_subscription_t;

typedef struct mqtt_connection {
    connection_t       *raw;
    mqtt_parser_t      *parser;
    mqtt_subscription_t subs[MAX_SUBS_PER_CONN];
    int                 sub_count;
    int                 fd;
    char                client_id[MQTT_ID_MAX];
    char                product_key[MQTT_ID_MAX];
    char                device_id[MQTT_ID_MAX];   /* CONNECT 认证时从 devices 表取回, 供 presence 标记用 */
    uint8_t             authenticated;
    uint8_t             connected;
    uint16_t            keepalive;
    time_t              last_active;
    char                will_topic[MQTT_ID_MAX];
    uint8_t            *will_payload;
    uint32_t            will_payload_len;
    uint8_t             will_qos;
} mqtt_connection_t;

typedef struct {
    connection_t      base;
    mqtt_connection_t mqtt;
} tcp_mqtt_conn_t;

typedef struct mqtt_session {
    int      used;
    int      fd;
    char     client_id[MQTT_ID_MAX];
    char     product_key[MQTT_ID_MAX];
    char     device_id[MQTT_ID_MAX];
    uint8_t  authenticated;
    uint16_t keepalive;
    time_t   last_active;
} mqtt_session_t;

#ifdef __cplusplus
}
#endif

#endif /* E2_MQTT_TYPES_H */
