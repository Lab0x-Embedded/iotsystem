# E2 — MQTT 协议详细设计

> **版本**: v3.0
> **更新日期**: 2026-10-06
> **协议版本**: MQTT 3.1.1（Protocol Level 4，其他版本拒绝）
> **端口**: 1883（config.json `mqtt_port`，启动日志会打印局域网 IP）

---

## 1. 概述

自研 MQTT Broker（kqueue/libevent 事件循环 + 线程池），支持：

- 设备认证（DB 查询 product_key + device_secret）
- 数据上报（QoS 0/1，QoS 1 回 PUBACK）
- **OneNET 物模型兼容上报**（`$sys/{pid}/{did}/thing/property/post` + OneJSON）
- 指令下发（QoS 1，离线队列 + 重连重放）
- 遗嘱消息、Keep Alive 超时踢线、同 client_id 重复连接踢旧
- 物模型属性白名单校验、未注册设备拒绝上报

---

## 2. 连接与认证

### 2.1 CONNECT 包

```
Variable Header:
  Protocol Name: "MQTT"
  Protocol Level: 0x04（当前实现仅接受 3.1.1）
  Connect Flags: Username + Password 必须置位
  Keep Alive: 建议 30~120 秒

Payload (各字段 ≤128 字节):
  Client ID: 任意唯一值，建议 esp8266_<device_id>
  Username:  product_key
  Password:  device_secret
```

### 2.2 认证逻辑（服务端实现在 mqtt_broker.c handle_connect）

```sql
SELECT device_id FROM devices
WHERE product_key = '<username>' AND device_secret = '<password>'
AND status IN ('registered', 'active')
LIMIT 1
```

- 命中 → CONNACK 0x00，连接成功后 presence 异步标记在线
- 凭证错误 / 设备未注册 / 状态不符 → CONNACK 0x04
- DB 不可用 → 认证缓存（TTL 10 分钟，条目来自真实认证成功）命中则放行，未命中 CONNACK 0x03
- **密钥按设备独立**：凭证 (product_key, device_secret) 唯一确定一台设备，连接认证身份即该 device_id，后续上报受身份绑定约束（见 §3.2 ⓪）
- **DB 不可用** → CONNACK 0x03（server unavailable，与凭证错误区分）
- **同 client_id 的旧连接会被新连接踢掉**（ESP8266 断线重连场景）

### 2.3 CONNACK 响应

| 返回码 | 含义 |
|--------|------|
| 0x00 | 连接成功 |
| 0x01 | 协议版本不支持（当前仅接受 level 4） |
| 0x03 | 服务不可用（数据库不可用） |
| 0x04 | 认证失败（凭证错误/设备未注册/状态不符） |

> 注：协议版本不匹配当前实际也返回 0x04（见 KNOWN_ISSUES M2 备注）。

---

## 3. 数据上报（原生 datapoints 协议）

### 3.1 Topic 与 Payload

```
topic: devices/{device_id}/data
```

```json
{
  "device_id": "dev_001",
  "datapoints": [
    {"metric": "temperature", "value": 25.3, "ts": 1720000001},
    {"metric": "humidity", "value": 65.0, "ts": 1720000001}
  ]
}
```

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| device_id | string | 是 | 已注册的设备 ID，**必须等于连接认证身份**（凭证解析出的 device_id），否则丢弃（身份绑定，防同产品设备冒充） |
| datapoints[].metric | string | 是 | 指标名（受产品物模型白名单约束，见 §3.3） |
| datapoints[].value | number | 是 | 指标值 |
| datapoints[].ts | number | 否 | Unix 秒，缺省用服务器时间 |

### 3.2 服务端处理流程（publish_worker，线程池执行）

```
PUBLISH 收到（主线程解析 JSON）
  ↓
⓪ 身份绑定校验：payload.device_id ≠ 连接认证身份 → 丢弃 + WARN
   （cmd 指令 topic 的设备侧 PUBLISH 也在此前被拒，见 §5）
  ↓
① 设备存在性校验：devices 表无此 (device_id, product_key) → 丢弃 + WARN
   （自动注册已移除，设备必须先经客户端/REST 注册）
  ↓
② 物模型白名单校验 thing_model_check：
   产品未定义任何属性 = 自由模式放行；
   定义后：白名单外 identifier 拒绝，bool 类型值非 0/1 拒绝
  ↓
③ 告警规则评估（alarm_evaluate_with_conn，复用当前 DB 连接）
  ↓
④ 写入月分表 data_reports_YYYYMM（表名由数据点 ts 推导，与查询分表路由
   同规则同 UTC；不存在则自动建表）
  ↓
⑤ upsert device_latest_data（按 device_id+metric 覆盖 value/ts）
```

QoS 1 的 PUBACK 在主线程发送，与上述入库异步。

### 3.3 物模型白名单（轻量）

- 配置：`POST /api/product` 的 `prop_add` / `prop_del` / `prop_list`（见 API 文档）
- 语义对齐 OneNET 10411：白名单外的 identifier 拒绝入库并记 WARN 日志
- 校验单点收口在 publish_worker，OneNET 协议与本节原生协议同等生效

---

## 4. OneNET 物模型兼容上报

topic：`$sys/{product_key}/{device_id}/thing/property/post`，payload 为 OneJSON：

```json
{"id": "1830", "params": {"temperature": {"value": 28.6}, "led1": {"value": "on"}}}
```

`params` 逐键值转 datapoint 走 §3.2 同一条入库链路：

| params 值形态 | 转换 |
|---------------|------|
| number | 直接取值 |
| bool | true→1 / false→0 |
| string | `"on"`/`"true"`→1，`"off"`/`"false"`→0，其余跳过 |
| object | 递归取 `value` 成员按上表转换（OneNET 嵌套写法） |

订阅侧 `$sys/...` 前缀与 `+` 单层通配符均支持（`thing/service/+/invoke` 可正常 SUBACK）。
完整设备侧接入说明见 `docs/E2_OneNET兼容接入.md` 与 `device_sdk/stm32_onenet/`。

---

## 5. 指令下发

```
topic: cmd/{device_id}/exec   (QoS 1)
```

```json
{"cmd": "set_relay", "payload": {"relay": "on"}}
```

- 设备**在线**：HTTP `POST /api/command` → 直接 PUBLISH，返回 `delivered`
- 设备**离线**：入离线队列，设备重连并订阅后自动重放，返回 `queued`
- **ACL**：cmd topic 只能订阅自己的（`cmd/<device_id>` 第 2 段 == 认证身份，
  通配符 `cmd/+` 拒绝，违规 SUBACK 0x80）；设备侧 PUBLISH 到 cmd topic 一律
  拒绝 —— 指令只由服务端直达目标连接注入，设备不可伪造下发给他设备

---

## 6. 连接保活与离线判定

- **Keep Alive 超时**：1.5 × keepalive（下限 5s）无任何报文 → 服务端关闭连接
- **异常断开**：socket 关闭即触发 `device_manager_offline`（presence 队列异步下刷）
- **巡检兜底**：presence 线程周期将 >90s 无活动的在线设备批量置离线

## 7. 遗嘱消息（Last Will）

CONNECT 可携带 Will Topic/Payload；设备异常断开时 broker 将遗嘱按订阅匹配转发。
设备的在线/离线状态由 §6 的 presence 链路维护，不依赖遗嘱。

---

## 8. 错误处理

| 错误 | 处理 |
|------|------|
| Protocol Level ≠ 4 | CONNACK 拒绝 |
| 认证失败（凭证/未注册） | CONNACK 0x04 |
| DB 不可用 | 认证缓存命中 → CONNACK 0x00；未命中 CONNACK 0x03 |
| SUBSCRIBE 未认证 / cmd topic 非自身 | SUBACK 0x80 |
| PUBLISH cmd topic（设备侧） | 拒绝 + WARN |
| payload device_id ≠ 认证身份 | 丢弃 + WARN |
| PUBLISH topic 含通配符 | 忽略 |
| payload JSON 解析失败 | 记日志忽略 |
| 设备未注册 | 丢弃 + WARN |
| metric 不在白名单 | 丢弃 + WARN（10411 语义） |

---

## 9. 客户端示例

```bash
# 随机/指定设备上报（Makefile）
make report                      # 随机设备上报
make report-dev DEV=dev_001      # 指定设备上报

# 模拟 ESP8266 全链路（认证→订阅→QoS1 上报→收指令）
python3 deploy/scripts/esp8266_e2e_test.py
python3 deploy/scripts/esp8266_e2e_test.py -d dev_005 --pk smart_meter --secret secret_005
```

设备侧完整参考实现（STM32 + ESP8266 AT）：`device_sdk/stm32_onenet/`。

---

## 10. 与标准 MQTT 的区别

| 特性 | 标准 MQTT Broker | E2 MQTT Broker |
|------|------------------|----------------|
| 认证 | 可配置 | DB 查询 product_key + device_secret |
| QoS | 0/1/2 | 0/1（2 不支持） |
| retain | 支持 | 不支持 |
| clean session | 支持 | 忽略（CONNACK session_present 恒 0） |
| 通配符订阅 | 支持 | 支持（`+` 单层 / `#` 末级） |
| 数据处理 | 透传 | JSON 解析 + 白名单校验 + 落库 + 告警评估 |
