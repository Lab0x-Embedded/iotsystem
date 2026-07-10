# IoT Device Platform

一个面向工业物联网场景的设备管理平台。

项目采用 **C 语言实现 MQTT Broker 核心服务**，结合 **Qt6 桌面客户端**，实现设备连接管理、消息通信、设备注册、数据管理等功能。

[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](https://opensource.org/licenses/MIT)
[![Platform](https://img.shields.io/badge/platform-Linux%20%7C%20macOS-lightgrey.svg)](https://github.com)
[![Tech](https://img.shields.io/badge/C-Qt6%20%7C%20MQTT%20%7C%20MySQL-green.svg)](https://github.com)

## 项目架构

### 系统架构图

```mermaid
flowchart LR
    subgraph Devices["设备层"]
        D1[IoT 设备]
        D2[模拟器<br/>mqtt_simulator]
    end

    subgraph Broker["Broker 服务层 / C 实现"]
        N[网络层<br/>TCP Socket]
        P[协议层<br/>CONNECT / PUBLISH / SUBSCRIBE]
        S[会话层<br/>Session Manager]
        R[路由层<br/>Topic Router]
        T[线程池<br/>pthread Pool]
    end

    subgraph Storage["数据层"]
        DB[(MySQL<br/>设备信息 + 消息记录)]
    end

    subgraph Client["管理端"]
        Qt[Qt6 桌面客户端<br/>设备监控 / 远程控制]
    end

    Devices -->|MQTT 1883| N
    N --> P
    P --> S
    S --> R
    R -->|持久化| DB
    R -->|下行推送| Qt
    Qt -->|控制下发| R
    T --> N
```

### 数据流

以一条设备上报消息为例：

```mermaid
sequenceDiagram
    participant D as IoT 设备
    participant B as MQTT Broker (C)
    participant DB as MySQL
    participant Q as Qt 客户端

    D->>B: PUBLISH topic=devices/001/status
    B->>B: Topic 路由匹配
    B->>DB: 持久化消息
    B-->>Q: 推送给订阅该 Topic 的客户端
    Q-->>B: SUBSCRIBE topic=devices/001/status
    B-->>Q: 下发匹配消息
    Q->>Q: UI 更新状态显示
```

## 技术栈

### 服务端

```mermaid
graph LR
    subgraph Core["核心"]
        C["C 语言<br/>高性能网络"]
        P["pthread<br/>多线程模型"]
    end
    subgraph Protocol["协议"]
        MQTT["MQTT 协议<br/>CONNECT / PUBLISH / SUBSCRIBE / PING"]
    end
    subgraph Infra["基础设施"]
        SOCK["Linux Socket"]
        JSON["cJSON<br/>配置解析"]
    end
    subgraph Persist["持久化"]
        MYSQL["MySQL 8.x<br/>Connector / C"]
    end
    subgraph Build["构建"]
        CMAKE["CMake ≥ 3.20"]
        GCC["GCC / Clang"]
    end
    Core --> Protocol --> Persist
    Core --> Persist
    Core --> Build
```

### 客户端

```mermaid
graph LR
    subgraph UI["界面层"]
        QML["QML + Qt Quick"]
        CPP["Qt C++ 业务"]
    end
    subgraph Net["通信"]
        TCP["TCP / MQTT Client"]
    end
    UI --> Net
```

### 数据库

| 组件 | 版本 | 用途 |
| --- | --- | --- |
| MySQL | 8.x | 设备信息、消息记录、状态历史 |

## 功能特性

### MQTT Broker

| 功能 | 说明 |
| --- | --- |
| TCP 连接管理 | 支持千级客户端并发连接 |
| CONNECT / CONNACK | 设备连接与认证 |
| PUBLISH / SUBSCRIBE | 发布订阅与 Topic 路由 |
| 心跳检测 | PINGREQ / PINGRESP 保活 |
| 多客户端并发 | pthread 线程池模型 |
| 消息广播 / 单播 | 高效分发 |

### 设备管理

- 设备注册与信息维护
- 在线 / 离线状态管理
- 模拟设备接入（Python 脚本）

### 数据管理

- MySQL 持久化
- 设备信息表 / 消息记录表 / 状态记录表

### Qt 管理端

- 设备列表展示
- 实时状态监控
- 远程控制指令下发
- 数据图表 / 日志查看

## 项目目录

```mermaid
graph TB
    Root(["e2-iot-device-platform /"])

    Root --> src["src 服务端源码"]
    src --> smqtt["mqtt 协议实现"]
    src --> snet["network 网络"]
    src --> sdb["database 数据库"]
    src --> sdev["device 设备管理"]
    src --> main["main.c"]

    Root --> inc["include 头文件"]
    Root --> client["client Qt 客户端"]

    Root --> deploy["deploy 部署与脚本"]
    deploy --> cfg["config.json 配置"]
    deploy --> sql["sql 数据库初始化"]
    deploy --> reg["register_devices.sh"]
    deploy --> sim["mqtt_simulator.py"]

    Root --> build["build 构建输出"]
    Root --> doc["doc / docs 文档"]
    Root --> cmake["CMakeLists.txt"]
    Root --> mk["Makefile"]
    Root --> readme["README.md"]
```

文字版目录结构：

```
e2-iot-device-platform
│
├── src/                 # Broker 源码
│   ├── mqtt/            # MQTT 协议实现
│   ├── network/         # 网络模块
│   ├── database/        # 数据库模块
│   ├── device/          # 设备管理
│   └── main.c
│
├── include/             # 头文件
│
├── client/              # Qt 客户端
│
├── deploy/
│   ├── config.json      # 服务配置
│   ├── sql/             # 数据库初始化
│   ├── register_devices.sh
│   └── mqtt_simulator.py
│
├── doc/                 # 项目文档
├── docs/                # 详细文档
├── build/               # 构建输出目录
├── CMakeLists.txt
├── Makefile
└── README.md
```

## 环境要求

### 服务端

```mermaid
graph LR
    subgraph Runtime["运行环境"]
        OS["Linux / macOS"]
        DB["MySQL 8.x"]
    end
    subgraph Build["构建工具"]
        CMake["CMake ≥ 3.20"]
        CC["GCC / Clang"]
        Make["GNU Make"]
    end
```

| 依赖 | 推荐版本 |
| --- | --- |
| OS | Linux / macOS |
| CMake | ≥ 3.20 |
| GCC / Clang | 支持 C11 |
| MySQL | 8.x |

### 客户端

| 依赖 | 推荐版本 |
| --- | --- |
| Qt | 6.x |
| CMake | ≥ 3.20 |
| C++ 编译器 | 支持 C++17 |

## 快速开始

### 1. 初始化数据库

```bash
make db_init
```

> 输入 MySQL root 密码。

### 2. 编译 Broker

```bash
make build
```

输出：`build/iot-broker`

### 3. 启动服务

```bash
make server
```

Console 输出：

```
MQTT Broker running...
listen port: 1883
```

### 4. 注册测试设备

```bash
make register_devices
```

### 5. 启动 MQTT 模拟器

```bash
make simulator
```

模拟多个 IoT 设备连接 Broker。

### 6. 启动 Qt 管理端

```bash
make client
```

构建并打开 `IoTDeviceManager.app`。

## 配置

配置文件路径：`deploy/config.json`

```json
{
    "mqtt_port": 1883,
    "http_port": 8080,
    "workers": 4,
    "backlog": 1024,
    "database": {
        "host": "127.0.0.1",
        "port": 3306,
        "user": "admin",
        "password": "123456",
        "database": "e2_iot",
        "pool_size": 4
    }
}
```

## 开发流程

完整的开发运行链路：

```mermaid
flowchart LR
    A["make build<br/>编译 Broker"] --> B["make db_init<br/>初始化数据库"]
    B --> C["make register_devices<br/>注册设备"]
    C --> D["make server<br/>启动 Broker"]
    D --> E["make simulator<br/>启动模拟器"]
    D --> F["make client<br/>启动 Qt 管理端"]
    E --> G((运行中))
    F --> G
```

```bash
# 1. 编译
make build

# 2. 初始化数据库
make db_init

# 3. 注册设备
make register_devices

# 4. 启动 Broker（终端 1）
make server

# 5. 启动模拟设备（终端 2）
make simulator

# 6. 启动 Qt 管理端（终端 3）
make client
```

## Makefile 命令

```mermaid
classDiagram
    class Makefile {
        +build 编译服务端
        +server 启动服务端
        +db_init 初始化数据库
        +register_devices 注册测试设备
        +client 编译并启动 Qt 客户端
        +simulator 启动 MQTT 模拟器
        +clean 清理构建文件
        +help 显示帮助
    }
```

| 命令 | 作用 |
| --- | --- |
| `make build` | 编译 IoT Broker 服务端 |
| `make server` | 启动服务端 |
| `make client` | 编译并启动 Qt 客户端 |
| `make db_init` | 初始化 MySQL 数据库 |
| `make register_devices` | 注册测试设备 |
| `make simulator` | 启动 MQTT 设备模拟器 |
| `make clean` | 清理构建目录 |
| `make help` | 查看帮助 |

## 性能目标

| 指标 | 目标 |
| --- | --- |
| MQTT 并发连接 | 1000+ |
| 消息吞吐 | 万级 / 秒 |
| 在线设备数 | 千级 |
| Broker 模型 | 多线程 pthread 池 |
| 数据存储 | MySQL 持久化 |

## Roadmap

### Phase 1 · 核心协议与连接

```mermaid
gantt
    title Phase 1 核心协议与连接
    dateFormat  YYYY-MM-DD
    section 网络层
    TCP 连接管理           :done, t1, 2026-06-01, 7d
    pthread 多线程模型      :done, t2, after t1, 5d
    section MQTT 协议
    CONNECT / CONNACK 解析  :done, t3, 2026-06-10, 4d
    PUBLISH 消息路由         :done, t4, after t3, 5d
    SUBSCRIBE 订阅管理       :done, t5, after t4, 3d
    PINGREQ 心跳检测         :done, t6, after t5, 2d
```

- [x] TCP 连接管理
- [x] MQTT 协议解析（CONNECT / PUBLISH / SUBSCRIBE / PING）
- [x] Client 认证与会话管理
- [x] Topic 订阅路由

### Phase 2 · 数据与客户端

```mermaid
gantt
    title Phase 2 数据与客户端
    dateFormat  YYYY-MM-DD
    section 持久化
    MySQL Connector/C 接入   :done, t1, 2026-06-20, 5d
    section 设备
    设备注册表 / 注册脚本     :done, t2, after t1, 4d
    在线 / 离线状态管理       :done, t3, after t2, 3d
    section 客户端
    Qt6 管理端基础框架       :done, t4, 2026-07-01, 10d
    MQTT 客户端通信          :done, t5, after t4, 5d
```

- [x] MySQL 设备管理
- [x] 设备注册
- [x] Qt 管理端基础框架

### Phase 3 · 进阶特性（规划中）

```mermaid
gantt
    title Phase 3 进阶特性
    dateFormat  YYYY-MM-DD
    section 消息
    消息持久化             :active, t1, 2026-07-15, 7d
    QoS 0 / 1 / 2 支持     :t2, after t1, 10d
    section 安全
    TLS 加密传输           :t3, 2026-08-01, 10d
    section 扩展
    集群 Broker           :t4, 2026-08-15, 14d
    Web 管理后台           :t5, 2026-09-01, 21d
```

- [ ] 消息持久化
- [ ] QoS 等级支持
- [ ] TLS 加密
- [ ] 集群 Broker
- [ ] Web 管理后台

## License

MIT License
