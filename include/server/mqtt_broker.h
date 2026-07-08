/**
 * @file mqtt_broker.h
 *
 * MQTT broker 入口.
 *
 * 设计:
 *   - broker 是无状态的全局对象 (单实例).
 *   - 表结构：session[] 与连接对象.
 *   - dispatch 路由.
 */

#ifndef E2_MQTT_BROKER_H
#define E2_MQTT_BROKER_H

#include "mqtt_types.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

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

/**
 * 注册/注销在线 MQTT 连接 (供 event_loop 在连接创建/销毁时调用).
 */
void mqtt_broker_register(mqtt_connection_t *conn);
void mqtt_broker_unregister(mqtt_connection_t *conn);

/**
 * 周期性 tick (1s 调用一次), 用于 keepalive 超时检测等.
 *
 *  @param now  当前时间 (time(NULL))
 */
void mqtt_broker_tick(time_t now);


#ifdef __cplusplus
}
#endif

#endif /* E2_MQTT_BROKER_H */
