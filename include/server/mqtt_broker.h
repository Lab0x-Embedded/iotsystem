/**
 * @file mqtt_broker.h
 *
 * MQTT broker 入口.
 *
 * 设计:
 *   - broker 是无状态的全局对象 (单实例).
 *   - 表结构：session[] 与[]  (连接对象).
 *   - dispatch 路由。
 */

#ifndef E2_MQTT_BROKER_H
#define E2_MQTT_BROKER_H

#include "mqtt_types.h"

/** broker 全局初始化. */
void mqtt_broker_init(void);

/**
 * 处理应用户端发送的完整一条报文.
 *
 *  @param conn  当前连接的 MQTT 上下文
 *  @param pkt   客户端发来的完整报文对象
 *
 *  pkt 所有权归 broker 处理: 内部会 unref pkt.
 */
void mqtt_broker_dispatch(mqtt_connection_t *conn, mqtt_packet_t *pkt);

#endif /* E2_MQTT_BROKER_H */
