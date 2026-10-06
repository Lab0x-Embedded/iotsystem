# OneNET 物模型兼容接入（路线 A）

平台兼容 OneNET Studio 的 MQTT 物模型 topic 与 OneJSON 信封 —— **OneNET 风格的设备端代码零改动**，
只改配置即可接入（已在 STM32 `app_onenet.c` 模式下验证）。

## 设备侧约定（与 OneNET 相同）

- **三元组**：client_id = 设备ID，username = 产品Key（对应 OneNET 的 product_id），password = 设备密钥
  （E2 不校验 OneNET Token，password 位直接放平台签发的设备密钥）
- **属性上报**：`$sys/{产品Key}/{设备ID}/thing/property/post`，
  payload 为 OneJSON：`{"id":"1700000001","params":{"temperature":26.5,"led1":"on"}}`
- **订阅**：`.../thing/property/post/reply`、`.../thing/property/set`、`.../thing/service/+/invoke`
  （`$sys` 与 `+` 通配符平台均支持）

## 参数转换规则

`params` 逐键值转成平台的 datapoint 入库（与 `devices/{id}/data` 同一条链路，告警/最新值/历史查询全兼容）：

| params 类型 | 转换 |
|-------------|------|
| number | 直接取值 |
| bool | true→1 / false→0 |
| string | `"on"`/`"true"`→1，`"off"`/`"false"`→0，其余跳过 |
| object `{"value": X}` | 递归取 `value` 成员按上表转换（OneNET 嵌套写法，实测常见） |

## STM32 工程改法（以 app_onenet.c 工程为例，仅改 app_config.h）

```c
#define APP_ONENET_PRODUCT_ID "factory_sensor"   /* E2 的产品 Key */
#define APP_ONENET_DEVICE_ID  "humi_temp"        /* E2 注册的设备 ID（client_id） */
#define APP_ONENET_TOKEN      "stm32test123"     /* E2 不校验 token，直接放设备密钥 */
#define ONENET_MQTT_SERVER    "192.168.0.49"     /* E2 服务器的局域网 IP */
#define ONENET_MQTT_PORT      1883
```

其余（订阅列表、OneJSON 组装、PUBRAW 上报、+MQTTSUBRECV 解析）全部保持不变。

## 物模型白名单（轻量约束）

产品可配置允许上报的属性（`POST /api/product`）：

- `prop_list` / `prop_add` / `prop_del`，字段：`product_key` + `identifier` + `prop_type`(number/bool/string)
- **产品未定义任何属性 = 自由模式**（全部放行，兼容既有演示设备）；
- 定义后：白名单外 identifier 拒绝入库（对齐 OneNET 10411 语义），bool 类型值非 0/1 拒绝；
- 校验在 publish_worker 单点收口，OneNET `params` 与原生 `datapoints` 两条链路同样生效；
- **设备必须先注册**：未注册设备的上报直接拒绝（不再自动注册）。

factory_sensor 已预置 7 个属性（temperature/humidity number，led1-3/buzzer/fan bool）。

## E2 侧暂不支持的 OneNET 特性

- Token 安全鉴权（HMAC-SHA1）—— 用设备密钥替代
- `thing/property/post/reply` 平台应答 —— 设备订阅了收不到，无碍
- `thing/property/set` 下行设置与服务调用下发 —— 平台暂不生成（指令下发仍走 `cmd/{id}/exec`）
- 物模型类型校验（10411/10415 错误码）—— params 不校验直接入库
