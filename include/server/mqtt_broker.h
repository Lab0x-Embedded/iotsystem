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
 * 查找在线连接 (线程安全).
 *
 *  @param client_id  设备的 MQTT client_id
 *  @return 在线连接指针, 或 NULL
 */
mqtt_connection_t *mqtt_broker_find_conn(const char *client_id);

/**
 * 按 device_id 查找在线连接 (线程安全).
 *
 * 注意: 连接的 client_id 与 device_id 通常不同
 *   (例如 client_id="esp8266_dev_001", device_id="dev_001")。
 * HTTP 指令接口拿到的是 device_id，必须用本函数查找。
 */
mqtt_connection_t *mqtt_broker_find_conn_by_device(const char *device_id);

/**
 * 向在线设备发送一条 QoS 1 命令 (PUBLISH).
 *
 *  @param conn         目标连接 (必须在线)
 *  @param topic        MQTT topic
 *  @param app_payload  应用层 payload 字节
 *  @param app_len      payload 长度
 *  @param out_pid      [out] 分配的 packet_id
 *  @return 0 成功, -1 失败
 */
int  mqtt_broker_send_cmd(mqtt_connection_t *conn,
                          const char *topic,
                          const uint8_t *app_payload,
                          uint32_t app_len,
                          uint16_t *out_pid);

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
