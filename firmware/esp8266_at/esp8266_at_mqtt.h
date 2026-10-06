/**
 * @file esp8266_at_mqtt.h
 *
 * ESP8266-01S（ESP-AT v2.2 固件）MQTT AT 指令的可移植参考驱动（C99，无平台依赖）。
 *
 * 移植方只需实现两个串口钩子（extern 声明见下）：
 *   at_uart_write()  阻塞写
 *   at_uart_read()   短超时读，返回实际读到的字节数（0 = 无数据）
 *
 * 典型主循环：
 *   esp_at_init(&cfg);            // 上电后一次：连 WiFi → 连 MQTT → 订阅
 *   while (1) {
 *       esp_at_poll(10);          // 泵串口，分发 +MQTTSUBRECV 指令回调
 *       ... 周期调用 esp_at_publish() 上报 ...
 *   }
 *
 * 注意：本驱动针对 ESP-AT v2.2.0.0_esp8266（LinkID 固定 0，MQTT over TCP）。
 */
#ifndef ESP8266_AT_MQTT_H
#define ESP8266_AT_MQTT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** 连接配置（字符串指针需在 esp_at_init 后保持有效） */
typedef struct {
    const char *wifi_ssid;
    const char *wifi_pass;
    const char *mqtt_host;      /* 平台地址，如 "192.168.1.100" */
    int         mqtt_port;      /* 默认 1883 */
    const char *client_id;      /* 建议 "esp8266_<设备ID>" */
    const char *username;       /* product_key */
    const char *password;       /* device_secret */
    int         keepalive_sec;  /* 建议 120；平台 1.5 倍超时判离线 */
} esp_at_config_t;

/** 平台下发指令回调：topic = "cmd/<设备ID>/exec"，payload 为平台 JSON */
typedef void (*esp_at_cmd_cb_t)(const char *topic, int topic_len,
                                const char *payload, int payload_len);

/** 用户移植的串口钩子（在主工程里实现） */
extern int at_uart_write(const char *data, int len);
extern int at_uart_read(char *out, int cap);

/** 注册指令回调（可在 init 前后任意时刻调用） */
void esp_at_set_cmd_cb(esp_at_cmd_cb_t cb);

/**
 * 完整初始化：AT 握手 → CWMODE → 连 WiFi → MQTT 配置/连接 → 订阅指令 topic。
 * 阻塞式，内部自带超时（总耗时约数秒，取决于 WiFi）。
 * @return 0 成功；-1 串口/模块无响应；-2 连 WiFi 失败；-3 连 MQTT 失败
 */
int esp_at_init(const esp_at_config_t *cfg);

/**
 * 上报一条数据（内部走 MQTTPUBRAW，自动计算长度并等待 +MQTTPUB:OK）。
 * @param json  原始 JSON 字节（不转义），如
 *              {"device_id":"dev_001","datapoints":[...]}
 * @return 0 成功；-1 超时/失败
 */
int esp_at_publish(const char *topic, const char *json, int len);

/**
 * 泵串口：分发 URC（+MQTTSUBRECV → 回调；+MQTTDISCONNECTED → 状态清零）。
 * @param timeout_ms  最长阻塞毫秒数（受 at_uart_read 实现影响）
 * @return 1 本轮收到过指令；0 无
 */
int esp_at_poll(int timeout_ms);

/** MQTT 是否处于已连接状态（+MQTTCONNECTED / +MQTTDISCONNECTED 维护） */
int esp_at_connected(void);

#ifdef __cplusplus
}
#endif

#endif /* ESP8266_AT_MQTT_H */
