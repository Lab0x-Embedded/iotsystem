# 已知问题与技术债（代码审计记录）

> 2026-10-06 文档-代码一致性审计时记录。按影响排序。
> ✅ = 已修复（2026-10-06 晚），修复时已同步文档。

## 🔴 高（安全）

| # | 问题 | 位置 | 说明 |
|---|------|------|------|
| H1 | JWT 认证是空桩 | `src/api/auth_middleware.c` | 只要有 `Bearer ` 前缀即放行，不校验签名/过期。登录返回的 `stub.jwt.<user>` 是开发桩。多用户/公网前必须实现 HMAC 校验 |
| H2 | 设备列表接口返回明文 device_secret | `handler_device.c` query/query_all/query_by_group | 客户端详情页展示需要。应有权限收紧（仅详情接口返回或按角色） |
| H3 | ~~ /api/onenet 旁路校验 ~~ | `handler_onenet.c` | ✅ 已修复：设备存在性校验（未注册 404）+ 白名单校验（product_key 取自设备记录），响应带 synced/rejected 计数 |

## 🟡 中（一致性/健壮性）

| # | 问题 | 位置 | 说明 |
|---|------|------|------|
| M1 | 无服务端推送通道 | 服务端整体 | 所有文档曾描述 SSE（`/api/sse`、`sse_handler.c`、客户端 `WsClient`），**均未实现**。客户端纯轮询。补齐 SSE/WebSocket 是体验上限的最大 unlock |
| M2 | ~~ CONNACK 拒绝码不区分原因 ~~ | `mqtt_broker.c` handle_connect | ✅ 已修复：DB 不可用 → 0x03（server unavailable），凭证/未注册 → 0x04。残留：协议版本不支持目前也走 0x04（原实现仅支持 level 4，如需放开 3/5 再细分） |
| M3 | DB 不可用时 MQTT 认证全拒 | `mqtt_broker.c` | 与 HTTP 侧（登录 503 + 醒目日志）语义不一致；可考虑缓存已认证设备或降级白名单 |
| M4 | ~~ REST 字段名不一致 ~~ | `handler_device.c` | ✅ 已修复：register 兼容 `name` 与 `device_name` 两种字段名 |
| M5 | ~~ MQTT SUBSCRIBE 不去重 ~~ | `mqtt_broker.c` handle_subscribe | ✅ 已修复：同连接同 topic 重复订阅改为覆盖 qos，不再占槽 |
| M6 | ~~ device_latest_data 无 ts 新旧比较 ~~ | `mqtt_broker.c` publish_worker | ✅ 已修复：upsert 加比较（`ts=GREATEST`，value 仅在 ts 更新时覆盖），旧时间戳不再回退最新值 |
| M7 | register 自动创建产品 | `handler_device.c` → bfac03c | 注册新产品 key 自动建产品（外键兜底），可能产生垃圾产品行，与产品管理页语义重叠 |

## 🟢 低（性能/整洁）

| # | 问题 | 位置 | 说明 |
|---|------|------|------|
| L1 | event loop 1000ms 硬超时 | `event_loop.c` | 主线程每秒空转唤醒；keepalive 典型 60~120s，可拉长 tick 间隔 |
| L2 | /api/onenet 名不符实 | `handler_onenet.c` | 只做心跳+告警不落库；要么补齐要么改名 datapoint_sync |
| L3 | 指令 topic 无 ACL | `mqtt_broker.c` | 设备认证后可订阅任意 topic（含其他设备的 cmd），多租户前需收敛 `cmd/<自己>/...` |
