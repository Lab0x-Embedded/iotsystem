/**
 * @file example_main.c
 *
 * 主控侧用法示意（STM32 HAL 为例，51 / Arduino 同理，替换两个串口钩子即可）。
 * 本文件不参与编译，仅作参考。
 *
 * 前置：ESP-01S 已刷 ESP-AT v2.2.0 固件（见 README.md 第 2 节）。
 */
#include <stdio.h>
#include "esp8266_at_mqtt.h"

/* ── 移植方需要实现的两根钩子（以 STM32 UART2 ↔ ESP-01S 为例） ── */

int at_uart_write(const char *data, int len)
{
    /* HAL_UART_Transmit(&huart2, (uint8_t *)data, len, 100); */
    (void)data; (void)len;
    return len;   /* 示意：实际返回发送成功的字节数 */
}

int at_uart_read(char *out, int cap)
{
    /* 短超时读：无数据返回 0
     * if (HAL_UART_Receive(&huart2, (uint8_t *)out, cap, 5) == HAL_OK) return cap;
     */
    (void)out; (void)cap;
    return 0;
}

/* ── 平台指令回调：解析 {"cmd":"set_relay","payload":{...}} ── */

static void on_command(const char *topic, int topic_len,
                       const char *payload, int payload_len)
{
    (void)topic; (void)topic_len;
    /* 简单示例：找 "relay":"on" / "off" 关键字控制 GPIO。
     * 正式项目建议上轻量 JSON 解析（如 cJSON）。 */
    if (payload_len <= 0) return;
    /* if (strstr_light(payload, "\"on\""))  relay_on();  */
    /* else if (strstr_light(payload, "\"off\"")) relay_off(); */
}

/* ── 主流程 ── */

void iot_task(void)
{
    static const esp_at_config_t cfg = {
        .wifi_ssid   = "YourWiFi",
        .wifi_pass   = "YourPass",
        .mqtt_host   = "192.168.1.100",   /* 平台所在主机 */
        .mqtt_port   = 1883,
        .client_id   = "esp8266_dev_001", /* 设备 ID = dev_001（去掉前缀） */
        .username    = "factory_sensor",  /* product_key */
        .password    = "secret_001",      /* device_secret，客户端设备详情页可复制 */
        .keepalive_sec = 120,
    };
    char json[160];
    int  retry = 0;

    esp_at_set_cmd_cb(on_command);

    /* 初始化失败按返回值区分：-1 模块无响应 / -2 WiFi / -3 MQTT */
    while (esp_at_init(&cfg) != 0) {
        if (++retry > 5) return;              /* 上报故障后交由上层处理 */
        HAL_Delay(3000);
    }

    /* 主循环：每 10 秒上报一次温度，poll 分发平台指令 */
    {
        int report_no = 0;
        for (;;) {
            int n;

            esp_at_poll(100);                 /* 泵串口 + 分发指令 */

            if (++report_no < 100) continue;  /* 100 × 100ms ≈ 10s */
            report_no = 0;

            if (!esp_at_connected()) {        /* 断线重连 */
                esp_at_init(&cfg);
                continue;
            }

            n = snprintf(json, sizeof(json),
                "{\"device_id\":\"dev_001\",\"datapoints\":"
                "[{\"metric\":\"temperature\",\"value\":25.6,\"ts\":1700000000}]}");
            esp_at_publish("devices/dev_001/data", json, n);
        }
    }
}
