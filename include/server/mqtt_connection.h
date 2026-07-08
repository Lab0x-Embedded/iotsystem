/**
 * @file mqtt_connection.h
 *
 * 把 connection_t 包装成 mqtt_connection_t.
 *
 * 设计:
 *   - 每个 accept 出来的 connection 都附带一个 mqtt_connection.
 *   - 协议自动检测: 第一个字节是 0x10 (CONNECT) → 进入 MQTT 模式;
 *     否则 → 保持 P1 echo 模式.
 *   - 进入 MQTT 模式后, 后续字节都走 mqtt_parser_feed.
 */

#ifndef E2_MQTT_CONNECTION_H
#define E2_MQTT_CONNECTION_H

#include "mqtt_types.h"

typedef enum {
    CONN_MODE_ECHO = 0,     /* 默认: P1 echo 模式              */
    CONN_MODE_MQTT,         /* 已进入 MQTT 模式                */
    CONN_MODE_CLOSING,      /* 已发 DISCONNECT, 等关闭          */
} conn_mode_t;

typedef struct {
    connection_t      *base;        /* 基类 socket 对象               */
    mqtt_connection_t  mqtt;        /* MQTT 上下文                    */
    conn_mode_t        mode;        /* 当前模式                       */
} conn_wrapper_t;

/** 创建 wrapper (base 由调用方提供). */
conn_wrapper_t *conn_wrapper_create(connection_t *base);

/** 销毁 wrapper (不关闭 base). */
void conn_wrapper_destroy(conn_wrapper_t *w);

/**
 * 喂入一个字节.
 *
 *  - 如果 mode == ECHO 且这是第一个字节:
 *      是 0x10 → 切到 MQTT 模式, 继续喂入.
 *      否则 → 把字节写回 base->rx_buf (echo 模式).
 *  - 如果 mode == MQTT → 喂入 parser, 完整报文走 broker 分发.
 *
 * 返回:
 *   0  正常
 *  -1  连接应该关闭
 */
int conn_wrapper_feed_byte(conn_wrapper_t *w, uint8_t b);

#endif /* E2_MQTT_CONNECTION_H */
