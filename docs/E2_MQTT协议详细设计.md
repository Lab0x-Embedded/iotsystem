# E2 — MQTT 协议详细设计

> **版本**: v2.0  
> **更新日期**: 2026-07-12  
> **协议版本**: MQTT 3.1.1 (Protocol Level 4)  
> **端口**: 1883 (可在 config.json 配置)

---

## 1. 概述

E2 IoT 平台使用自定义 MQTT Broker 实现设备接入，支持：

- 设备认证 (用户名/密码)
- 数据上报 (PUBLISH)
- 指令下发 (PUBLISH)
- 遗嘱消息 (Last Will)
- QoS 0

---

## 2. 连接与认证

### 2.1 CONNECT 包

设备连接时必须携带认证信息：

```
Fixed Header: 0x10 + Remaining Length
Variable Header:
  Protocol Name: "MQTT" (4 bytes)
  Protocol Level: 0x04 (MQTT 3.1.1)
  Connect Flags: 0xC0 (Username + Password)
  Keep Alive: 60 seconds

Payload:
  Client ID: device_id
  Username: product_key
  Password: device_secret
```

### 2.2 认证逻辑

```c
// mqtt_broker.c
#define AUTH_PRODUCT_KEY   "pk_test"
#define AUTH_DEVICE_SECRET "secret_001"

int ok = (strcmp(username, AUTH_PRODUCT_KEY) == 0) &&
         (strcmp(password, AUTH_DEVICE_SECRET) == 0);
```

### 2.3 CONNACK 响应

| 返回码 | 含义 |
|--------|------|
| 0x00 | 连接成功 |
| 0x05 | 认证失败 |

---

## 3. 数据上报

### 3.1 Topic 格式

```
devices/{device_id}/data
```

示例: `devices/D001/data`

### 3.2 Payload 格式

```json
{
  "device_id": "D001",
  "datapoints": [
    {
      "metric": "temperature",
      "value": 25.3,
      "ts": 1720000001
    },
    {
      "metric": "humidity",
      "value": 65.0,
      "ts": 1720000001
    }
  ]
}
```

### 3.3 字段说明

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| device_id | string | 是 | 设备 ID |
| datapoints | array | 是 | 数据点数组 |
| datapoints[].metric | string | 是 | 指标名 |
| datapoints[].value | number | 是 | 指标值 |
| datapoints[].ts | number | 否 | 时间戳 (Unix 秒)，为空则使用服务器时间 |

### 3.4 服务端处理流程

```
PUBLISH 收到
  ↓
解析 JSON payload
  ↓
① 自动注册设备 (INSERT IGNORE INTO devices)
  ↓
② 告警规则评估 (alarm_evaluate)
  ↓
③ 广播到 SSE 客户端 (sse_broadcast_datapoint)
  ↓
④ 写入 MySQL 分表 (INSERT INTO data_YYYYMM)
  ↓
⑤ 更新设备影子 (shadow_update_reported)
```

---

## 4. 指令下发

### 4.1 Topic 格式

```
cmd/{device_id}/exec
```

示例: `cmd/D001/exec`

### 4.2 Payload 格式

```json
{
  "cmd": "reboot",
  "payload": {"force": true}
}
```

### 4.3 下发流程

```
HTTP POST /api/command
  ↓
查找设备连接 (mqtt_broker_find_conn)
  ↓
┌─────────────────┬─────────────────┐
│   设备在线       │   设备离线       │
├─────────────────┼─────────────────┤
│ 直接 PUBLISH     │ 入队离线队列     │
│ 返回 "delivered" │ 返回 "queued"   │
└─────────────────┴─────────────────┘
```

---

## 5. 遗嘱消息 (Last Will)

### 5.1 配置

设备在 CONNECT 包中设置遗嘱：

```
Will Topic: devices/{device_id}/status
Will Payload: {"online": false}
Will QoS: 0
Will Retain: 0
```

### 5.2 触发条件

- 设备异常断开连接
- 服务端在 Keep Alive 超时后未收到 PINGREQ

---

## 6. Keep Alive

- **默认超时**: 60 秒
- **PINGREQ/PINGRESP**: 设备定期发送 PINGREQ，服务端回复 PINGRESP
- **超时处理**: 服务端关闭连接，触发遗嘱消息

---

## 7. 错误处理

| 错误 | 处理 |
|------|------|
| 协议版本不支持 | 返回 CONNACK + 错误码 0x01 |
| 认证失败 | 返回 CONNACK + 错误码 0x05 |
| Topic 格式无效 | 忽略 PUBLISH |
| JSON 解析失败 | 记录日志，忽略 |

---

## 8. 客户端示例

### 8.1 Python (原生 socket)

```python
import socket, struct, json, time

def mqtt_connect(sock, client_id, username, password):
    vh = b"\x00\x04MQTT\x04\xc0\x00\x3C"
    cid = client_id.encode()
    user = username.encode()
    pwd = password.encode()
    payload = struct.pack("!H", len(cid)) + cid
    payload += struct.pack("!H", len(user)) + user
    payload += struct.pack("!H", len(pwd)) + pwd
    remaining = len(vh) + len(payload)
    pkt = b"\x10" + struct.pack("!B", remaining) + vh + payload
    sock.sendall(pkt)
    resp = sock.recv(4)
    return len(resp) >= 4 and resp[3] == 0x00

def mqtt_publish(sock, topic, payload):
    topic_b = topic.encode()
    data_b = payload.encode()
    remaining = 2 + len(topic_b) + len(data_b)
    hdr = b"\x30" + struct.pack("!B", remaining)
    sock.sendall(hdr + struct.pack("!H", len(topic_b)) + topic_b + data_b)

# 使用
sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
sock.connect(("127.0.0.1", 1883))
mqtt_connect(sock, "my_device", "pk_test", "secret_001")

data = {
    "device_id": "D001",
    "datapoints": [
        {"metric": "temperature", "value": 25.3, "ts": int(time.time())}
    ]
}
mqtt_publish(sock, "devices/D001/data", json.dumps(data))
```

### 8.2 项目内脚本

```bash
# 使用 Makefile
make report                      # 随机设备上报
make report-dev DEV=dev_001      # 指定设备上报

# 直接运行
python3 deploy/scripts/mqtt_ss_report.py -d dev_001 -i 1
```

---

## 9. 与标准 MQTT 的区别

| 特性 | 标准 MQTT Broker | E2 MQTT Broker |
|------|------------------|----------------|
| 认证 | 可配置 | 硬编码 product_key/secret |
| 持久化 | 可选 | 无 (内存) |
| QoS | 0/1/2 | 仅 QoS 0 |
| 订阅 | 支持 | 支持 (用于指令下发) |
| 数据处理 | 透传 | 自动解析 JSON + 落库 + 告警评估 |
