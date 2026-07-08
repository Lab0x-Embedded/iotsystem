/**
 * @file mqtt_connection.c
 */
#include "mqtt_connection.h"
#include "mqtt_broker.h"
#include "mqtt/mqtt_parser.h"
#include "mqtt/mqtt_codec.h"
#include "common/log.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>

conn_wrapper_t *conn_wrapper_create(connection_t *base) {
    conn_wrapper_t *w = calloc(1, sizeof(*w));
    if (!w) return NULL;
    w->base = base;
    w->mode = CONN_MODE_ECHO;
    w->mqtt.fd = base->fd;
    w->mqtt.raw = base;
    return w;
}

void conn_wrapper_destroy(conn_wrapper_t *w) {
    if (!w) return;
    if (w->mqtt.parser) mqtt_parser_destroy(w->mqtt.parser);
    free(w);
}

/* 进入 MQTT 模式 */
static int enter_mqtt_mode(conn_wrapper_t *w) {
    w->mqtt.parser = mqtt_parser_create();
    if (!w->mqtt.parser) return -1;
    w->mode = CONN_MODE_MQTT;
    return 0;
}

/* 把 base->rx_buf 里 [rx_off, rx_len) 这一段喂入 parser */
static int __attribute__((unused)) feed_parser_from_buf(conn_wrapper_t *w) {
    connection_t *b = w->base;
    while (b->rx_off < b->rx_len) {
        int have = mqtt_parser_feed(w->mqtt.parser, b->rx_buf[b->rx_off]);
        b->rx_off++;
        if (have) {
            mqtt_packet_t *pkt = mqtt_parser_take(w->mqtt.parser);
            if (!pkt) {
                LOG_ERROR("parser take failed");
                return -1;
            }
            mqtt_broker_dispatch(&w->mqtt, pkt);
            /* broker_dispatch 内部 unref pkt */
            if (w->mode == CONN_MODE_CLOSING) return -1;
        }
    }
    b->rx_off = b->rx_len = 0;
    return 0;
}

int conn_wrapper_feed_byte(conn_wrapper_t *w, uint8_t b) {
    switch (w->mode) {
    case CONN_MODE_ECHO:
        if (b == 0x10) {
            /* 第一个字节就是 CONNECT → 切到 MQTT 模式 */
            if (enter_mqtt_mode(w) != 0) return -1;
            LOG_INFO("fd=%d switch to MQTT mode", w->base->fd);
            return conn_wrapper_feed_byte(w, b);
        }
        /* echo 模式: 把字节写回 rx_buf */
        if (w->base->rx_len >= w->base->rx_cap) {
            size_t nc = w->base->rx_cap * 2;
            unsigned char *nb = realloc(w->base->rx_buf, nc);
            if (!nb) return -1;
            w->base->rx_buf = nb;
            w->base->rx_cap = nc;
        }
        w->base->rx_buf[w->base->rx_len++] = b;
        return 0;

    case CONN_MODE_MQTT:
        if (!w->mqtt.parser) {
            if (enter_mqtt_mode(w) != 0) return -1;
        }
        if (mqtt_parser_feed(w->mqtt.parser, b) == 1) {
            mqtt_packet_t *pkt = mqtt_parser_take(w->mqtt.parser);
            if (!pkt) return -1;
            mqtt_broker_dispatch(&w->mqtt, pkt);
            if (w->mode == CONN_MODE_CLOSING) return -1;
        }
        return 0;

    case CONN_MODE_CLOSING:
        return -1;
    }
    return 0;
}
