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
| M1 | ~~ 无服务端推送通道 ~~ | 服务端整体 | ❌ 决定不做(2026-10-07)：客户端维持纯 REST 轮询，不再规划 SSE/WebSocket。历史背景：早期文档曾虚构 SSE（`/api/sse`、`sse_handler.c`、客户端 `WsClient`），均未实现，相关描述已从全部文档清除 |
| M2 | ~~ CONNACK 拒绝码不区分原因 ~~ | `mqtt_broker.c` handle_connect | ✅ 已修复：DB 不可用 → 0x03（server unavailable），凭证/未注册 → 0x04。残留：协议版本不支持目前也走 0x04（原实现仅支持 level 4，如需放开 3/5 再细分） |
| M3 | ~~ DB 不可用时 MQTT 认证全拒 ~~ | `mqtt_broker.c` handle_connect | ✅ 已修复(2026-10-07)：新增认证缓存（真实 DB 认证成功时记录 pk+secret→device_id，TTL 10min，128 条 LRU），DB 故障窗口内缓存命中放行（回 CONNACK ACCEPTED），未命中仍回 0x03。折衷：期间被禁用/删除的设备最多在 TTL 窗口内还能重连成功。缓存仅事件循环线程访问，无锁 |
| M4 | ~~ REST 字段名不一致 ~~ | `handler_device.c` | ✅ 已修复：register 兼容 `name` 与 `device_name` 两种字段名 |
| M5 | ~~ MQTT SUBSCRIBE 不去重 ~~ | `mqtt_broker.c` handle_subscribe | ✅ 已修复：同连接同 topic 重复订阅改为覆盖 qos，不再占槽 |
| M6 | ~~ device_latest_data 无 ts 新旧比较 ~~ | `mqtt_broker.c` publish_worker | ✅ 已修复：upsert 加比较（`ts=GREATEST`，value 仅在 ts 更新时覆盖），旧时间戳不再回退最新值 |
| M7 | ~~ register 自动创建产品 ~~ | `handler_device.c` → bfac03c | ❌ 决定不做(2026-10-07)：保留自动建产品作为外键兜底（注册新产品 key 时自动创建，避免注册失败）。客户端注册界面已引导先建产品，垃圾产品行风险可控 |
| M9 | 同产品设备可互相冒充上报 | `mqtt_broker.c` handle_publish / `init_data.sql` | datapoints 路径 device_id 取自 payload 而非连接认证身份；同产品设备共享 device_secret（init_data 每产品一个密钥），dev_001 的连接可自称 dev_002 上报（跨产品会被 publish_worker 的 pk 配对校验拦截）。协议级修复需：每设备独立密钥 + payload device_id 与认证身份绑定校验——会破坏现有固件契约与 e2_report.py 场景脚本（单连接报多设备），留待协议 v4 一并做 |
| M8 | ~~ 上报压死连接池（嵌套占用 + 锁内等池）~~ | `mqtt_broker.c` publish_worker → `thing_model.c` | ✅ 已修复(2026-10-07)：publish_worker 已持 1 条池连接，`thing_model_check` 又在 `g_mtx` 锁内 `db_pool_get()` 抢第二条（cache_load），4 worker × 2 > 池 4 条；且持锁等池最多 3s 串死全部上报线程，加载失败时 `loaded` 保持 0 → 每条上报重试查库，恶性循环（设备上报期间持续 `db_pool: 4/4 in use, timeout waiting 3s`）。修复：新增 `thing_model_check_with_conn()` 复用调用方已持连接加载缓存、DB 访问移出 `g_mtx`、加载失败退避 5s；顺带修掉 TTL 到期 cache_load 连调两次。auto-resolve 的 UPDATE 仅 `mysql_affected_rows>0` 时打日志（原每条回落上报刷一条） |

## 🟢 低（性能/整洁）

| # | 问题 | 位置 | 说明 |
|---|------|------|------|
| L7 | 入库表选择与查询分表路由不一致 | `mqtt_broker.c` publish_worker / `data/shard_router.c` | publish_worker 的月表名按**墙钟**当月选择（`CREATE TABLE IF NOT EXISTS data_reports_YYYYMM LIKE template` + INSERT），而 query_history 按**数据点 ts** 路由月表。补报历史 ts 的数据点会写进当前月表、按 ts 查询却路由到旧月表 → 查不到（total=-1 时通常是目标月表不存在）。修复方向：入库表名改由 datapoint ts 推导，或查询回退扫描相邻月表 |

| # | 问题 | 位置 | 说明 |
|---|------|------|------|
| L6 | ~~ 动态绑定内引用 QtShadcn 枚举首轮 undefined ~~ | 各 View 的 `variant:` 三元绑定 | ✅ 已缓解(2026-10-07)：qmlcachegen 只编译期解析**静态**枚举赋值，`cond ? ShadcnButton.Variant.Default : ...` 这类**动态绑定里的枚举引用**首轮运行期求值为 undefined → 每次启动刷 4~6 条 `Unable to assign [undefined] to int`，且 undefined 写回 int 被跳过、属性保持原值（表现为筛选 chip 无选中态）。已统一改用整数序数+注释。序数对照 `ShadcnButton` 枚举：**Primary=0, Secondary=1, Outline=2, Ghost=3, Destructive=4, Link=5**（历史写法 `? 0 : 1 /* Outline */` 的 1 实为 Secondary，已纠正为 `? 0 : 2`）。**规范：动态绑定内不要引用 QtShadcn 枚举，改用序数+注释**；上游正解是让 qmlcachegen 支持动态枚举解析 |

| # | 问题 | 位置 | 说明 |
|---|------|------|------|
| L5 | ~~ lastSeen 兜底伪装"刚刚" + report_count 恒 0 ~~ | `datamanager.cpp` / `publish_worker` | ✅ 已修复(2026-10-07)：ts==0 保持无效显示"-"；新增 devices.last_report_at 列(migration)，publish_worker 入库成功时 report_count+1 + 刷新 last_report_at；REST query/query_all/query_by_group 透出 last_report；客户端详情页区分「最后在线/最后上报」 |

| # | 问题 | 位置 | 说明 |
|---|------|------|------|
| L1 | ~~ event loop 1000ms 硬超时 ~~ | `event_loop.c` | ✅ 已修复(2026-10-07)：空闲等待 1s→5s。tick 只承担 keepalive 超时检测（判定下限本就是 5s），报文/新连接/停机(self-pipe) 到来时 kqueue 立即唤醒，不影响响应性 |
| L2 | ~~ /api/onenet 名不符实 ~~ | `handler_onenet.c` | ✅ 已修复(2026-10-07)：datapoint 改经 `mqtt_broker_submit_datapoint()` 走与 MQTT 上报同一条 publish_worker 链路（注册校验/物模型/告警评估/分表入库/latest 更新全兼容）；告警评估移入 worker（原先 handler 直调 alarm_evaluate，同一数据点会重复评估、连续告警计数翻倍）。已验证 REST 注入的 datapoint 可 query_history 查到 |
| L3 | ~~ 指令 topic 无 ACL ~~ | `mqtt_broker.c` | ✅ 已修复(2026-10-07)：① SUBSCRIBE 要求先完成 CONNECT 认证；② cmd topic 只能订阅自己的（cmd/<device_id> 第 2 段 == 认证身份，通配符 cmd/+ 也拒），否则 SUBACK 0x80；③ 设备侧 PUBLISH 到 cmd topic 一律拒绝（指令只由服务端 mqtt_broker_send_cmd 直达目标连接注入，不经过 PUBLISH 转发路径）。已实测：自身 cmd 放行 / 他人 cmd 与 cmd/+ 拒绝 / PUBLISH cmd 打 denied 日志 |
