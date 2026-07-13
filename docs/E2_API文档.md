# E2 — REST API 接口文档

> **版本**: v2.0  
> **更新日期**: 2026-07-12  
> **Base URL**: `http://127.0.0.1:8080/api`  
> **认证方式**: Bearer Token (JWT)  
> **Content-Type**: `application/json`  
> **请求方式**: 所有接口均为 POST，通过 `action` 字段区分操作

---

## 目录

1. [认证接口](#1-认证接口)
2. [设备管理接口](#2-设备管理接口)
3. [告警接口](#3-告警接口)
4. [设备影子接口](#4-设备影子接口)
5. [指令下发接口](#5-指令下发接口)
6. [分组接口](#6-分组接口)
7. [SSE 实时推送](#7-sse-实时推送)
8. [错误码](#8-错误码)

---

## 1. 认证接口

### POST /api/user

#### action: login

用户登录，获取 Token。

**请求**:
```json
{
  "action": "login",
  "username": "admin",
  "password": "admin@123"
}
```

**响应** `200 OK`:
```json
{
  "token": "stub.jwt.admin",
  "role": "admin",
  "user_id": 1
}
```

**响应** `401 Unauthorized`:
```json
{
  "error": "invalid credentials"
}
```

---

## 2. 设备管理接口

### POST /api/device

#### action: register — 注册设备

**请求**:
```json
{
  "action": "register",
  "device_id": "D001",
  "name": "温度传感器",
  "product_key": "pk_test",
  "device_type": "sensor",
  "device_secret": "secret_001",
  "group_id": 1
}
```

**响应** `200 OK`:
```json
{"status": "registered"}
```

#### action: query — 查询单个设备

**请求**:
```json
{"action": "query", "device_id": "D001"}
```

**响应** `200 OK`:
```json
{
  "device_id": "D001",
  "name": "温度传感器",
  "product_key": "pk_test",
  "device_type": "sensor",
  "device_secret": "secret_001",
  "group_id": 1,
  "state": 1,
  "online": true,
  "last_active": 1720000000,
  "last_online": 1720000000,
  "updated_at": 1720000000,
  "report_count": 1234
}
```

#### action: query_all — 查询所有设备

**请求**:
```json
{"action": "query_all"}
```

**响应** `200 OK`:
```json
{
  "data": [
    {
      "device_id": "D001",
      "name": "温度传感器",
      "product_key": "pk_test",
      "group_id": 1,
      "state": 1,
      "online": true,
      "report_count": 1234
    }
  ],
  "total": 10,
  "online_count": 7
}
```

#### action: update — 更新设备信息

**请求** (更新分组):
```json
{"action": "update", "device_id": "D001", "group_id": 2}
```

**请求** (更新名称):
```json
{"action": "update", "device_id": "D001", "name": "新名称"}
```

**响应** `200 OK`:
```json
{"status": "updated"}
```

#### action: query_by_group — 按分组查询

**请求**:
```json
{"action": "query_by_group", "group_id": 1}
```

**响应** `200 OK`:
```json
{"data": [...], "total": 5, "group_id": 1}
```

#### action: query_history — 查询历史数据

**请求**:
```json
{
  "action": "query_history",
  "device_id": "D001",
  "metric": "temperature",
  "start_ts": 1720000000,
  "end_ts": 1720086400,
  "limit": 200
}
```

**响应** `200 OK`:
```json
{
  "data": [
    {"ts": 1720000001, "value": 25.30},
    {"ts": 1720000003, "value": 25.50}
  ],
  "total": 150
}
```

---

## 3. 告警接口

### POST /api/alarm

#### action: query — 查询告警记录

**请求**:
```json
{"action": "query"}
```

**响应** `200 OK`:
```json
{
  "data": [
    {
      "id": 1,
      "deviceId": "D001",
      "metric": "temperature",
      "currentValue": 36.50,
      "threshold": 35.00,
      "severity": 2,
      "status": "active",
      "acknowledged": false,
      "acknowledgedBy": 0,
      "acknowledgedAt": "",
      "resolvedBy": 0,
      "resolvedAt": "",
      "triggeredAt": 1720000000
    }
  ]
}
```

**status 字段说明**:
| 值 | 说明 |
|-----|------|
| `active` | 活跃（未确认） |
| `acknowledged` | 已确认 |
| `resolved` | 已解决 |

**severity 字段说明**:
| 值 | 说明 |
|-----|------|
| 0 | Info（信息） |
| 1 | Warning（警告） |
| 2 | Critical（严重） |

#### action: acknowledge — 确认告警

**请求**:
```json
{"action": "acknowledge", "id": 1, "user_id": 1}
```

**响应** `200 OK`:
```json
{"status": "acknowledged"}
```

#### action: resolve — 解决告警

**请求**:
```json
{"action": "resolve", "id": 1, "user_id": 1}
```

**响应** `200 OK`:
```json
{"status": "resolved"}
```

#### action: add_rule — 添加告警规则

**请求**:
```json
{
  "action": "add_rule",
  "device_id": "D001",
  "metric": "temperature",
  "op": 0,
  "threshold": 35.0,
  "severity": 2
}
```

**op 字段说明**:
| 值 | 说明 |
|-----|------|
| 0 | GT（大于） |
| 1 | LT（小于） |
| 2 | EQ（等于） |
| 3 | GTE（大于等于） |
| 4 | LTE（小于等于） |

**响应** `200 OK`:
```json
{"status": "added"}
```

#### action: query_rules — 查询告警规则

**请求**:
```json
{"action": "query_rules"}
```

**响应** `200 OK`:
```json
{
  "data": [
    {
      "id": 1,
      "deviceId": "D001",
      "metric": "temperature",
      "op": 0,
      "threshold": 35.0,
      "severity": 2,
      "enabled": true
    }
  ]
}
```

#### action: toggle_rule — 切换规则启用状态

**请求**:
```json
{"action": "toggle_rule", "id": 1}
```

**响应** `200 OK`:
```json
{"status": "toggled"}
```

#### action: edit_rule — 编辑规则

**请求**:
```json
{
  "action": "edit_rule",
  "id": 1,
  "device_id": "D001",
  "metric": "temperature",
  "op": 0,
  "threshold": 40.0,
  "severity": 2
}
```

**响应** `200 OK`:
```json
{"status": "updated"}
```

#### action: delete_rule — 删除规则

**请求**:
```json
{"action": "delete_rule", "id": 1}
```

**响应** `200 OK`:
```json
{"status": "deleted"}
```

---

## 4. 设备影子接口

### POST /api/shadow

#### 查询影子 (无 action)

**请求**:
```json
{"device_id": "D001"}
```

**响应** `200 OK`:
```json
{
  "device_id": "D001",
  "version": 3,
  "desired": {"temperature": 25, "fan_speed": "high"},
  "reported": {"temperature": 23.5, "humidity": 65}
}
```

#### action: set_desired — 设置单个期望值

**请求**:
```json
{"action": "set_desired", "device_id": "D001", "key": "temperature", "value": "25"}
```

**响应** `200 OK`:
```json
{"status": "desired_set"}
```

#### action: update — 批量更新期望值

**请求**:
```json
{
  "action": "update",
  "device_id": "D001",
  "desired": {
    "temperature": 25,
    "fan_speed": "high"
  }
}
```

**响应** `200 OK`:
```json
{"status": "updated", "updated_keys": 2}
```

#### action: delta — 查询差异

**请求**:
```json
{"action": "delta", "device_id": "D001"}
```

**响应** `200 OK`:
```json
{
  "delta": [
    {"key": "temperature", "desired": "25"}
  ]
}
```

---

## 5. 指令下发接口

### POST /api/command

**请求**:
```json
{
  "device_id": "D001",
  "cmd": "reboot",
  "payload": {"force": true}
}
```

**设备在线** — 响应 `200 OK`:
```json
{"status": "delivered"}
```

**设备离线** — 响应 `202 Accepted`:
```json
{"status": "queued", "command_id": "cmd_1720000000"}
```

**发送失败** — 响应 `500 Internal Error`:
```json
{"error": "send failed"}
```

---

## 6. 分组接口

### POST /api/group

#### action: query_all — 查询所有分组

**请求**:
```json
{"action": "query_all"}
```

**响应** `200 OK`:
```json
{
  "data": [
    {
      "group_id": 1,
      "parent_id": 0,
      "group_name": "工厂A",
      "description": "生产基地A",
      "device_count": 5,
      "sort_order": 1
    }
  ],
  "total": 9
}
```

| action: create | 创建分组（仅一级） |
| POST `{"action":"create","name":"新分组","description":"描述"}` | `200 {"status":"created","group_id":5}` |
**请求**:
```json
{"action": "create", "name": "新分组", "description": "描述"}
```

**响应** `200 OK`:
```json
{"status": "created", "group_id": 10}
```

#### action: update — 更新分组

**请求**:
```json
{"action": "update", "group_id": 1, "group_name": "新名称", "description": "新描述"}
```

**响应** `200 OK`:
```json
{"status": "updated"}
```

#### action: delete — 删除分组

**请求**:
```json
{"action": "delete", "group_id": 1}
```

**响应** `200 OK`:
```json
{"status": "deleted"}
```

---

## 7. SSE 实时推送

### GET /api/sse

SSE (Server-Sent Events) 端点，客户端通过 HTTP 长连接接收实时数据。

**请求**:
```
GET /api/sse HTTP/1.1
Host: 127.0.0.1:8080
Accept: text/event-stream
Cache-Control: no-cache
```

**响应**:
```
HTTP/1.1 200 OK
Content-Type: text/event-stream
Cache-Control: no-cache
Connection: keep-alive
Access-Control-Allow-Origin: *

: connected

event: datapoint
data: {"type":"datapoint","device_id":"D001","metric":"temperature","value":25.30,"ts":1720000001}

event: datapoint
data: {"type":"datapoint","device_id":"D001","metric":"humidity","value":65.00,"ts":1720000001}

event: alarm
data: {"type":"alarm","device_id":"D001","metric":"temperature","value":36.50,"threshold":35.00,"severity":2}
```

**事件类型**:
| event | 说明 | 数据字段 |
|-------|------|----------|
| `datapoint` | 设备数据点 | device_id, metric, value, ts |
| `alarm` | 告警事件 | device_id, metric, value, threshold, severity |

---

## 8. 错误码

| HTTP 状态码 | 说明 |
|-------------|------|
| 200 | 成功 |
| 202 | 已接受（离线指令入队） |
| 400 | 请求参数错误 |
| 401 | 认证失败 |
| 404 | 资源不存在 |
| 405 | 方法不允许 |
| 500 | 服务器内部错误 |
| 503 | 服务不可用（SSE 客户端满） |

**通用错误响应格式**:
```json
{"error": "错误描述"}
```
