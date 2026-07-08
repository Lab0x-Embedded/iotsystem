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
    char                client_id[65];
    uint8_t             authenticated;
    uint8_t             connected;
} mqtt_connection_t;

typedef struct {
    connection_t      base;
    mqtt_connection_t mqtt;
} tcp_mqtt_conn_t;

typedef struct mqtt_session {
    int      used;
    int      fd;
    char     client_id[65];
    char     product_key[33];
    char     device_id[33];
    uint8_t  authenticated;
    uint16_t keepalive;
    time_t   last_active;
} mqtt_session_t;

#ifdef __cplusplus
}
#endif

#endif /* E2_MQTT_TYPES_H */
