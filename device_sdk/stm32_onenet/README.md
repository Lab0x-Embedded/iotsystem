# STM32 设备侧参考实现（OneNET 物模型 → E2 平台）

> **这是设备侧参考代码，不参与 E2 平台的构建。** 源自 STM32F407 工程
> `stm32mx407test_xinxin`（ESP8266-01S AT 固件 + OneNET Studio 物模型协议），
> 收录于此作为"设备如何接入本平台"的完整参考。真实开发请在原 STM32 工程进行。

## 分层职责

| 文件 | 层 | 职责 |
|------|-----|------|
| `app_config.h` | 配置 | 唯一真值源：WiFi/三元组/上报周期/物模型清单/调试开关 |
| `bsp_esp8266.c/.h` | BSP | ESP8266 AT 串口驱动：指令应答匹配、`+MQTTSUBRECV` 按声明长度解析、PUBRAW 握手+送数+等 ACK、丢包/错误计数 |
| `app_wifi.c/.h` | 网络 | WiFi 连接（`AT+CWJAP`） |
| `app_onenet.c/.h` | 协议 | MQTT 会话、`$sys/{pid}/{did}` topic 组装、OneJSON 属性上报、下行处理表分派、`+MQTTDISCONNECTED` 状态维护 |
| `app_device.c/.h` | 业务 | 物模型属性处理（led1-3/buzzer/fan/DHT11）、property/set 应答、服务调用 |

## 接入 E2 平台（只改 `app_config.h`，零代码改动）

E2 平台已兼容 OneNET 物模型的 topic 与 OneJSON 信封（见仓库
`docs/E2_OneNET兼容接入.md`），设备侧五项配置照抄即可：

```c
#define APP_ONENET_PRODUCT_ID "factory_sensor"    /* E2 的产品 Key（username） */
#define APP_ONENET_DEVICE_ID  "esp8266_dev_001"   /* E2 注册的设备 ID（client_id） */
#define APP_ONENET_TOKEN      "<设备密钥>"         /* E2 不校验 token，放设备密钥即可 */
#define ONENET_MQTT_SERVER    "192.168.0.49"      /* E2 服务器局域网 IP */
#define ONENET_MQTT_PORT      1883
```

其余（订阅列表、OneJSON `{"id":..,"params":{..}}` 组装、PUBRAW 上报、
`+MQTTSUBRECV` 按长度解析）全部保持不变；`params` 支持扁平值和
`{"value": X}` 嵌套两种形态，数字/bool/`"on"-"off"` 字符串平台都会入库。

## 原工程依赖（本目录副本未包含）

`main.h`（STM32 HAL：`HAL_GetTick`/`HAL_Delay`/UART 句柄）、DHT11/LCD 等外设
BSP —— 以原工程为准。本目录代码**不能也不需要**被平台编译。

## 协议要点（踩坑记录）

- `AT+MQTTPUBRAW` 的长度声明必须等于 payload 真实字节数，先算后拼，
  否则静默截断、极难排查（`app_onenet.c` 的 `OneNET_PublishAbsolute`）；
- Token/密码过长的截断会以 CONNACK 拒绝的形式呈现，入口处显式检查长度；
- 服务下行的 topic 必须订到 `/thing/service/<id>/invoke` 这一层（`+` 只吃一层）；
- 每次连接先 `AT+MQTTDISCONN` + `AT+MQTTCLEAN` 清模块状态机，避免"必须断电重启"。
