#ifndef __APP_ONENET_H
#define __APP_ONENET_H

#include "main.h"
#include <stddef.h>

/* ============================================================
 *  OneNET MQTT 通用层
 *
 *  职责边界（重要）：
 *    - 本层只做：AT/MQTT 会话、topic 组装、发布、订阅、下行报文解析与分派
 *    - 本层**不认识** LED / 蜂鸣器 / 风扇 / DHT11 / LCD，也不 include cJSON
 *      —— 下行 payload 原样交给注册进来的业务处理器
 *    - 业务处理器在 APP/Src/app_device.c 里注册（OneNET_SetHandlers）
 * ============================================================ */

#include "app_config.h"

/* 单条下行 payload 上限（超过直接丢弃并打日志） */
#define ONENET_RX_MAX 512

typedef enum {
  ONENET_OK = 0,
  ONENET_ERR_INIT,
  ONENET_ERR_AT,
  ONENET_ERR_CONNECT,
  ONENET_ERR_SUBSCRIBE,
  ONENET_ERR_SEND,
  ONENET_ERR_TIMEOUT,
  ONENET_ERR_NOT_CONNECTED
} ONENET_Status_t;

/* 下行处理器：payload 已补 '\0'，len 是报文里声明的字节数（两个都给，按需取）*/
typedef void (*OnenetTopicHandler)(const char *topic, const char *payload,
                                   size_t len);

/* 处理表：topic_suffix 与 topic 尾部**精确匹配**（如 "/thing/property/set"）。
 * 若 topic 尾部带变量（如 cmd/request/<id>），把 match_anywhere 置 1，
 * 改为子串匹配。 */
typedef struct {
  const char *topic_suffix;
  OnenetTopicHandler handler;
  uint8_t match_anywhere;
} OnenetHandler;

/* ---------------- 会话 ----------------
 * 前置条件：ESP8266 已初始化、已连上 WiFi（WIFI_Init / WIFI_Connect）*/

/* 配置 MQTT 客户端（清旧连接 + MQTTUSERCFG）。不碰 WiFi 参数。 */
ONENET_Status_t OneNET_MqttInit(const char *product_id, const char *device_id,
                                const char *token);

/* 连接服务器 + 订阅默认 topic 列表 */
ONENET_Status_t OneNET_MqttConnect(void);

/* 主动断开（并清连接状态） */
void OneNET_Disconnect(void);

uint8_t OneNET_IsConnected(void);

/* ---------------- 收发 ---------------- */

/* 注册下行处理表（在 Connect 之前调） */
void OneNET_SetHandlers(const OnenetHandler *table, size_t count);

/* 发布到 $sys/<pid>/<did><topic_suffix>，topic_suffix 以 '/' 开头 */
ONENET_Status_t OneNET_Publish(const char *topic_suffix, const char *payload);

/* 发布到已经拼好的完整 topic（服务应答这类"topic 带变量"的场景用） */
ONENET_Status_t OneNET_PublishAbsolute(const char *topic, const char *payload);

/* 物模型属性上报：拼 {"id":"<tick>","params":<params_json>} 发到 thing/property/post */
ONENET_Status_t OneNET_PublishProperty(const char *params_json);

/* 主循环里轮询：解析 +MQTTSUBRECV 下行并按处理表分派。
 * 返回 1 表示本轮有下行被处理表命中（未命中的只打日志）。 */
uint8_t OneNET_Poll(void);

#endif /* __APP_ONENET_H */
