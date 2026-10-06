# ESP8266-01S（AT 固件）接入指南

适用场景：ESP-01S 作为 **WiFi 协处理器**挂在 51 / STM32 / Arduino 等主控的 UART 上，
主控通过 AT 指令驱动它连本平台的 MQTT broker。
若 ESP-01S 独立工作（自己跑业务逻辑），直接刷 [firmware/esp8266/](../esp8266/) 的
Arduino 固件更省事，不需要本文档。

---

## 1. 硬件准备与接线

| ESP-01S 引脚 | 接法 | 说明 |
|--------------|------|------|
| VCC | 3.3V **独立供电 ≥300mA** | 峰值电流大，禁止从开发板 3.3V 引脚偷电 |
| GND | 共地（与主控、USB-TTL 三方共地） | |
| TX | → 主控 RX / USB-TTL RX | 115200 8N1 |
| RX | ← 主控 TX / USB-TTL TX | **3.3V 电平**，5V MCU 需分压 |
| EN(CH_PD) | 拉高（10k 上拉到 3.3V） | |
| IO0 | 悬空（运行）/ 拉低（烧录） | 上拉 10k 更稳 |
| IO2 | 悬空 / 上拉 | |
| RST | 悬空或接主控 GPIO 复位 | |

## 2. 刷 ESP-AT v2.2.0 固件（关键步骤）

出厂自带的旧版 AT 固件（NONOS 1.7.x）**没有 MQTT 指令**，必须刷乐鑫官方
ESP-AT v2.2.0.0 for ESP8266。

**先确认版本**（115200 波特率，回车换行结尾发指令）：

```text
AT+GMR
```

响应里没有 `2.2.0.0` 字样就需要刷固件：

1. 下载官方 release：`https://github.com/espressif/esp-at/releases`
   找 **v2.2.0.0_esp8266** 的压缩包（ESP8266-IDF-AT）。
2. ESP-01S 进烧录模式：**IO0 拉低 → RST 拉一下复位 → 松开**。
3. 烧录（release 包里的 `factory.bin` 已含 bootloader + 分区表，1MB flash 一把烧）：

```bash
esptool.py --chip esp8266 --port /dev/tty.usbserial-XXXX \
    --baud 921600 write_flash --flash_mode dio --flash_size 1MB 0x0 factory.bin
```

> 具体偏移以 release 包内 `README.md` / flash 参数文件为准。
4. IO0 恢复悬空，复位。`AT+GMR` 应返回 `AT version:2.2.0.0`。

## 3. 接入本平台的 AT 指令序列

本平台参数与协议的对应关系（详情见客户端「设备 → 接入」页，会按设备自动填充）：

| MQTT 元素 | 本平台约定 |
|-----------|-----------|
| client_id | 任意 ≤128 字节唯一值，建议 `esp8266_<设备ID>`（重复连接自动踢旧连接） |
| username  | 设备所属产品的 **product_key** |
| password  | **device_secret**（客户端设备详情页可复制） |
| 上报 topic | `devices/<设备ID>/data`，QoS 0/1 |
| 指令 topic | `cmd/<设备ID>/exec`，QoS 1（订阅） |

完整序列（`<...>` 替换成实际值）：

```text
AT+CWMODE=1
AT+CWJAP="<WiFi名>","<WiFi密码>"
AT+MQTTUSERCFG=0,1,"esp8266_<设备ID>","<product_key>","<device_secret>",0,0,""
AT+MQTTCONNCFG=0,120,1,"","",0,0
AT+MQTTCONN=0,"<平台IP>",1883,1
AT+MQTTSUB=0,"cmd/<设备ID>/exec",1
```

成功标志：`AT+MQTTCONN` 后收到 `+MQTTCONNECTED:0`；平台侧设备详情页状态转在线。

## 4. 上报数据：用 MQTTPUBRAW，不要用 MQTTPUB

AT 指令行有约 256 字节上限，且 JSON 里的 `"` 要转义成 `\"`，`AT+MQTTPUB`
很容易写不下/转义错。**用 MQTTPUBRAW 发原始字节**：

```text
AT+MQTTPUBRAW=0,"devices/dev_001/data",86,1,0
```

模块回 `OK` 后出现 `>` 提示符，此时**原样发送** 86 字节 payload（不转义、不加回车）：

```json
{"device_id":"dev_001","datapoints":[{"metric":"temperature","value":25.6,"ts":1759709400}]}
```

发完等 `+MQTTPUB:OK`（QoS 1 时平台侧 broker 会回 PUBACK，客户端无需关心）。
`86` 必须等于 payload 的真实字节数。

## 5. 接收平台指令

设备在线时，从客户端或 `curl -X POST http://<平台IP>:8080/api/command ...` 下发指令，
串口会收到 URC：

```text
+MQTTSUBRECV:0,"cmd/dev_001/exec",42,{ "cmd": "set_relay", "payload": { "relay": "on" } }
```

格式：`+MQTTSUBRECV:<LinkID>,<"topic">,<字节数>,<原始数据>`。
**payload 数据是原样的、可能包含 `\r\n`，不能按行解析** —— 要先解析出
`<字节数>`，再从串口流里按字节读满该数量（驱动已实现，见下）。

## 6. 可移植驱动

[`esp8266_at_mqtt.c/.h`](esp8266_at_mqtt.c) 是一个无平台依赖的 C99 参考实现
（STM32 HAL / Arduino C++ / 51 移植均可），用户只需提供两个串口钩子：

```c
int at_uart_write(const char *data, int len);  // 阻塞写
int at_uart_read(char *out, int cap);          // 短超时读，返回实际字节数
```

用法见 [`example_main.c`](example_main.c)。
Keil C51 编译器不支持 C99，移植时把 `stdint.h` 换成 `reg51.h` + 自定类型即可，逻辑不变。

## 7. 常见问题

| 现象 | 原因 / 处理 |
|------|-------------|
| `AT+MQTTUSERCFG` 直接 ERROR | 固件不是 ESP-AT v2.x（旧版 AT 无 MQTT 指令），回第 2 节刷固件 |
| `+MQTTCONN:FAIL` | 平台 IP/端口错；broker 没启动；或 secret 错（服务端日志会打 `auth failed for user=...`） |
| `+MQTTDISCONNECTED:0` 频繁出现 | 同 client_id 的另一个连接把这台踢下线；或供电不稳导致模块重启 |
| 收不到 +MQTTSUBRECV | 没订阅成功；指令 topic 不是 `cmd/<自己的设备ID>/exec` |
| CWJAP FAIL | WiFi 是 5GHz（ESP8266 仅 2.4GHz）；密码错；信号弱 |
| 串口乱码 | 波特率不是 115200；晶振不稳；供电不足 |
