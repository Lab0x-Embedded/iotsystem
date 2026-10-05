# ESP8266 硬件接入 + SSE→轮询 + QtShadcn UI 重构 Implementation Plan

> **For agentic workers:** 按 Phase 顺序执行（A → B → C），每步用 checkbox (`- [ ]`) 跟踪；改动独立可验证，小步提交。

**Goal:** Phase A 砍掉 SSE 改轮询（简化架构），Phase B 用 QtShadcn 重构客户端 UI，Phase C 让 ESP8266 通过 WiFi 接入平台。

**Architecture:** Phase A 删除服务端 SSE 模块并新增 `query_latest` REST 接口，客户端用 QTimer 轮询替代 SSE 长连接。Phase B 以 submodule 方式引入 QtShadcn，`QQuickStyle::setStyle("Basic")`，逐页面用 Shadcn 组件替换手写 QML。Phase C 服务端 MQTT 兼容性修补 + ESP8266 Arduino 客户端示例。

**Tech Stack:** 服务端 C11 + libevent + cJSON + MySQL；客户端 C++17 / Qt6 / QML / QtShadcn；硬件 ESP8266 Arduino core + PubSubClient。

**Spec:** 设计已在 chat 中确认（SSE→轮询 + QtShadcn 逐页面迁移 + ESP8266 接入）。

## Global Constraints

- 服务端 `-Wall -Wextra -Werror` 零警告，每个 Task `make build` 通过。
- 客户端 C++17 / Qt 6.5+，每个 Task `cd client && cmake --build build` 通过。
- QtShadcn 接入铁律：`third_party/qtshadcn` submodule + `add_subdirectory` + `QQuickStyle::setStyle("Basic")` + `QML_IMPORT_PATH`。
- 不改变 MQTT 对外协议语义（Phase C 除外）。
- 每个 Phase 独立可编译可运行。

---

## 进度快照（2026-10-06）

| Phase | 状态 | 说明 |
|---|---|---|
| A: SSE → 轮询 | ✅ 完成 | 服务端删 SSE 模块；新增 `query_latest`；客户端 `WsClient` 删除，改 3s `QTimer` 轮询 |
| B: QtShadcn UI | ✅ 完成（方案有调整） | 见下方「Phase B 实际落地」 |
| C: ESP8266 接入 | ✅ 完成 | 固件示例 + 服务端兼容性 + e2e 脚本，全部验证通过 |

### Phase B 实际落地（与原计划不同之处）

原计划是「逐页面把 QML 换成 Shadcn 组件」，实际做的时候发现原结构本身有不合理处，于是做了信息架构重构：

- 6 个页面 → **3 个视图 + 1 个详情页**：`DevicesView` / `AlarmsView` / `GroupsView` / `DeviceDetailView`
- 原 `OverviewPage + DashboardPage` 首页数据是**写死的假数据**（`rand()`、硬编码 gauge），已改为 `query_latest` 真实数据
- 原 `GroupManagePage + GroupDetailPage` 合并为单页 master-detail
- 新增 `components/Panel.qml`：`ShadcnCard` 的 `implicitHeight` 依赖内部 Column，列表用 `anchors.fill` 会形成尺寸环（表现 CPU 100% 卡死），列表/图表容器统一改用 Panel
- 新增 `components/AppSidebar.qml`，导航不再用魔法数字 `currentIndex` 跳转
- 指标不再写死：`DataManager` 新增 `metricNamesFor/latestValue/metricSummary/metricLabel/metricUnit`
- 修复：`ShadcnDialogFooter` 必须挂在 `ShadcnDialogContent` 上（挂在 `ShadcnDialog` 上会落到 `QQC.Dialog` 自己的 footer，不右对齐）
- 恢复被重写时丢失的表头；选择状态由「行号」改为「id」（模型 5s 轮询重置后行号会错位）

### 待办（Backlog，非阻塞）

- [ ] **规则表批量删除的 N 次请求**：现在批量删除 N 条规则会发 N 次 HTTP。
      规则量大时给服务端加批量 action（如 `/api/alarm {"action":"delete_rules","ids":[...]}`）
- [ ] 客户端 TLS（MQTT 1883 / HTTP 8080 目前明文）
- [ ] 设备影子 delta 在客户端的可视化（目前只展示 reported/desired 原文）

---

---

# Phase A: SSE → 轮询

### Task A1: 服务端删除 SSE 模块

**Files:**
- Delete: `src/server/sse_handler.c`, `include/server/sse_handler.h`
- Modify: `src/main.c`（删 include + sse_handler_init 调用）
- Modify: `src/server/mqtt_broker.c`（删 include + sse_broadcast_datapoint 调用）

- [x] 删除 sse_handler.c/h
- [x] main.c 删 `#include "server/sse_handler.h"` 和 sse_handler_init 块
- [x] mqtt_broker.c 删 include 和 sse_broadcast_datapoint 调用（保留周围的 datapoint 处理逻辑）
- [x] `make build` 通过
- [x] 提交 `refactor(server): 删除 SSE 模块，客户端改用 HTTP 轮询`

### Task A2: 服务端新增 query_latest 接口

**Files:**
- Modify: `src/api/handlers/handler_device.c`（新增 action="query_latest"）

**Interfaces:**
- Produces: `POST /api/device {"action":"query_latest"}` → `{"data":[{"device_id":"...","metric":"...","value":...,"ts":...},...],"total":N}`（一次读全 device_latest_data 表）

- [x] handler_device.c 新增 query_latest 分支：`SELECT device_id, metric, value, ts FROM device_latest_data`
- [x] `make build` + curl 验证
- [x] 提交 `feat(api): 新增 query_latest 返回全量设备最新数据点`

### Task A3: 客户端删除 WsClient + 加轮询

**Files:**
- Delete: `client/src/network/wsclient.h`, `client/src/network/wsclient.cpp`
- Modify: `client/src/main/datamanager.h/cpp`（QTimer 轮询 fetchDevices + fetchLatest + fetchAlarms）
- Modify: `client/src/network/httpclient.h/cpp`（新增 fetchLatest）
- Modify: `client/src/main/main_qml.cpp`（移除 wsclient 注册）
- Modify: `client/qml/main.qml`（连接状态改 HTTP 成功标记）

- [x] httpclient 新增 `fetchLatest()` Q_INVOKABLE 调用 `/api/device action=query_latest`
- [x] DataManager 加 `QTimer m_pollTimer`（3s），connect 到 fetchDevices + fetchLatest + fetchAlarms
- [x] 删除 wsclient 文件 + main_qml.cpp 里的注册 + main.qml 里 wsclient 引用
- [x] `cd client && cmake --build build` 通过
- [x] 提交 `refactor(client): 删除 SSE WsClient，改为 3s QTimer 轮询 REST`

---

# Phase B: QtShadcn UI 重构

### Task B1: 引入 QtShadcn submodule + CMake 接线

**Files:**
- Create: `client/third_party/qtshadcn` (git submodule)
- Modify: `client/CMakeLists.txt`
- Modify: `client/src/main/main_qml.cpp`
- Modify: `Makefile`（client target 加 QML_IMPORT_PATH）

- [x] `git submodule add https://github.com/QtShadcn/qtshadcn.git client/third_party/qtshadcn`
- [x] client/CMakeLists.txt: find_package 加 Svg；`add_subdirectory(third_party/qtshadcn)`；target_link_libraries 加 QtShadcn；cmake_minimum_required 改 3.24
- [x] main_qml.cpp: `QQuickStyle::setStyle("Basic")`（替换 Material）
- [x] Makefile client/client-dev: 加 `QML_IMPORT_PATH=$(pwd)/client/build/third_party/qtshadcn/src`
- [x] `cd client && cmake -B build && cmake --build build` 通过
- [x] 提交 `feat(client): 引入 QtShadcn submodule + Basic style + CMake 接线`

### Task B2: main.qml 主题入口 + 替换 Material

**Files:**
- Modify: `client/qml/main.qml`
- Modify: `client/qml/qml.qrc`

- [x] main.qml: 删 Material import，`import QtShadcn`，加 `QtShadcnTheme { id: theme }`
- [x] 颜色属性改为绑定 theme token（theme.background / theme.card / theme.border 等）
- [x] 删除本地 isDark/cardBg/accentColor 等重复属性
- [x] client/src/theme/ThemeManager 用法替换（QML 引用改到 QtShadcn 的 theme 对象）
- [x] qml.qrc 保留（main.qml 不用改入口方式）
- [x] 编译通过 + `make client` 手动验证窗口能打开
- [x] 提交 `refactor(client/main): QtShadcnTheme 主题入口替换 Material 样式`

### Task B3: Overview 页面迁移

**Files:**
- Modify: `client/qml/pages/OverviewPage.qml`
- Modify: `client/qml/components/StatCard.qml`（重写为 ShadcnCard 组合）
- Modify: `client/qml/components/TopDevicesTable.qml`（重写为 ShadcnTable）

- [x] StatCard → ShadcnCard { Header: ShadcnCardTitle + CardDescription; Content: 大字数值 + ShadcnBadge 状态 }
- [x] TopDevicesTable → ShadcnTable + ShadcnTableModel（C++ model 不变，只换 QML 视图）
- [x] StatusIndicator → ShadcnStatusDot
- [x] RoundedButton → ShadcnButton
- [x] 编译通过 + 页面渲染正常
- [x] 提交 `refactor(client/overview): Overview 页迁移到 QtShadcn`

### Task B4: Detail 页面迁移

**Files:**
- Modify: `client/qml/pages/DetailPage.qml`（912 行）

- [x] 数据卡片 → ShadcnCard 组合
- [x] GaugeCard/GaugeWidget 保留 Qt Charts/Canvas 绘图但外壳换 ShadcnCard
- [x] 指令发送按钮 → ShadcnButton
- [x] 阈值输入 → ShadcnInput + ShadcnSlider
- [x] 编译通过
- [x] 提交 `refactor(client/detail): Detail 页迁移到 QtShadcn`

### Task B5: AlarmCenter 页面迁移

**Files:**
- Modify: `client/qml/pages/AlarmCenterPage.qml`
- Modify: `client/qml/components/AddAlarmRuleDialog.qml`

- [x] 告警列表 → ShadcnTable 或 ShadcnCard 列表 + ShadcnBadge 状态标签
- [x] AddAlarmRuleDialog → ShadcnDialog + ShadcnInput/Select/Slider
- [x] 确认/解决按钮 → ShadcnButton（variant primary / destructive）
- [x] 规则启用开关 → ShadcnSwitch
- [x] 编译通过
- [x] 提交 `refactor(client/alarm): AlarmCenter 页迁移到 QtShadcn`

### Task B6: Dashboard / GroupManage / GroupDetail 迁移

**Files:**
- Modify: `client/qml/pages/DashboardPage.qml`
- Modify: `client/qml/pages/GroupManagePage.qml`
- Modify: `client/qml/pages/GroupDetailPage.qml`

- [x] Dashboard: MetricCard → ShadcnCard，RealtimeChart 外壳 → ShadcnCard
- [x] GroupManage: 分组列表 → ShadcnCard + ShadcnButton 操作，创建对话框 → ShadcnDialog
- [x] GroupDetail: 同 Detail 模式
- [x] 编译通过
- [x] 提交 `refactor(client/pages): Dashboard/GroupManage/GroupDetail 迁移到 QtShadcn`

### Task B7: 清理旧组件 + theme 模块

**Files:**
- Delete: `client/qml/components/MetricCard.qml`, `DashboardCard.qml`, `GaugeCard.qml`, `StatCard.qml`, `RoundedButton.qml`, `StatusIndicator.qml`
- Delete: `client/src/theme/theme.h`, `client/src/theme/theme.cpp`
- Modify: `client/qml/qml.qrc`
- Modify: `client/src/main/main_qml.cpp`（删 themeManager 注册）

- [x] 删除已被 Shadcn 替换的旧 QML 组件
- [x] 删除项目自己的 ThemeManager（QtShadcn 自带）
- [x] 更新 qml.qrc 和 main_qml.cpp
- [x] 编译通过
- [x] 提交 `chore(client): 清理被 QtShadcn 替换的旧组件和 ThemeManager`

---

### Phase C 实际落地（与原计划不同之处）

- **协议版本**：原计划写「接受 protocol_level 3~5」。实际解析器是按 MQTT 3.1.1 定长可变头硬编码的
  （协议名固定 4 字节 `MQTT`、keepalive 之后直接是 Client ID），3.1（`MQIsdp`）与 5（多了 properties）
  布局不同，仅放宽判断会让报文解析错位。ESP8266 + PubSubClient 用的正是 3.1.1，因此保持只支持 level 4，
  改为输出可操作提示。
- **C4 测试额外发现并修复了 3 个真实 bug**（原计划未预见，均已在指令链路上验证）：
  1. `handler_command` 取 `payload->valuestring`：客户端发的是 JSON 对象，对象时该字段为 NULL，
     `strlen(NULL)` 会崩溃 → 改为 cJSON 原样序列化
  2. 在线判定用错 key：`mqtt_broker_find_conn()` 按 client_id 查，HTTP 传的是 device_id
     （client_id 通常是 `esp8266_<device_id>`）→ 在线设备被误判为离线入队。新增 `mqtt_broker_find_conn_by_device()`
  3. QoS>0 的 PUBLISH 未跳过 packet_id：JSON 从 packet_id 开始解析必然失败；转发给订阅者时也没剥掉
     packet_id（订阅者会把它当成数据）
  4. 离线队列 key 不一致：入队用 device_id、出队用 client_id，重放永远匹配不上 → 统一用 device_id

---

# Phase C: ESP8266 硬件接入

### Task C1: 服务端 MQTT 兼容性修补

**Files:**
- Modify: `include/mqtt/mqtt_types.h`
- Modify: `src/server/mqtt_broker.c`

**Interfaces:**
- Produces: client_id/product_key/device_id buffer 128B；protocol_level 3-5 接受；同 client_id 重复 CONNECT 踢旧连接。

- [x] mqtt_types.h: `char client_id[129]` / `product_key[129]` / `device_id[129]`
- [x] mqtt_broker.c handle_connect: buffer 同步放宽 + `protocol_level < 3 || > 5` 拒绝（而非只接受 4）
- [x] 同 client_id 重复连接: `mqtt_broker_find_conn` 找到旧连接 → `old->connected = 0; shutdown(old->fd, SHUT_RDWR);`
- [x] `make build` 通过
- [x] 提交 `feat(mqtt): 兼容 ESP8266 — buffer 128B / protocol_level 3-5 / 踢旧连接`

### Task C2: ESP8266 Arduino 客户端示例

**Files:**
- Create: `firmware/esp8266/esp8266_sensor_relay/esp8266_sensor_relay.ino`
- Create: `firmware/esp8266/README.md`

- [x] .ino: WiFi 连接 + PubSubClient MQTT + DHT11 上报（topic `devices/{id}/data`）+ 订阅 `cmd/{id}/exec` 控制继电器
- [x] 认证: username=product_key, password=device_secret
- [x] README: 接线图 + 库安装 + 配置说明 + FAQ
- [x] 提交 `feat(firmware): ESP8266 温湿度上报 + 继电器控制示例`

### Task C3: 指令下发 payload 对齐

**Files:**
- Modify: `src/api/handlers/handler_command.c`

- [x] send_cmd 的 payload 包装为 `{"cmd":"...","payload":"..."}` JSON（当前直接发原始字符串）
- [x] `make build` 通过
- [x] 提交 `fix(command): payload 统一 JSON 包装`

### Task C4: 端到端模拟验证脚本

**Files:**
- Create: `deploy/scripts/esp8266_e2e_test.py`

- [x] 模拟 ESP8266: CONNECT(认证) → SUBSCRIBE(cmd topic) → PUBLISH(datapoints QoS1) → 验证 PUBACK
- [x] 提交 `test: ESP8266 接入端到端模拟脚本`

### Task C5: README 更新

**Files:**
- Modify: `README.md`

- [x] 硬件接入章节补充 firmware 路径 + e2e 验证命令
- [x] 提交 `docs(readme): 补充 ESP8266 硬件接入说明`
