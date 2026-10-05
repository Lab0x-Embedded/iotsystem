# ESP8266 接入指南

用一块 ESP8266 + DHT11 接入本平台：上报温湿度、接收指令控制继电器。

## 1. 硬件

| 元件 | 说明 |
|---|---|
| ESP8266 NodeMCU V3 / Wemos D1 mini | 主控 |
| DHT11 或 DHT22 | 温湿度传感器 |
| 继电器模块（可选） | 控制负载 |
| 面包板 + 杜邦线 | — |

### 接线

| 模块 | 接到 ESP8266 | 说明 |
|---|---|---|
| DHT DATA | `D2` (GPIO4) | 数据脚，代码里 `DHT_PIN` |
| DHT VCC | `3.3V` | |
| DHT GND | `GND` | DHT11 建议 DATA 与 VCC 之间加 4.7k~10k 上拉 |
| 继电器 IN | `D1` (GPIO5) | 代码里 `RELAY_PIN` |
| 继电器 VCC | `VIN` / `5V` | 多数继电器模块要 5V |
| 继电器 GND | `GND` | 与开发板共地 |

> 继电器有高电平触发和低电平触发两种，代码里 `RELAY_ACTIVE_HIGH` 按你的模块改（默认 1=高电平触发）。

## 2. 软件

1. **Arduino IDE** → 文件 → 首选项 → 附加开发板管理器网址填：
   `https://arduino.esp8266.com/stable/package_esp8266com_index.json`
2. 工具 → 开发板 → 开发板管理器 → 搜索 `esp8266` → 安装 **esp8266 by ESP8266 Community**
3. 开发板选 **NodeMCU 1.0 (ESP-12E Module)**
4. 工具 → 管理库 → 搜索并安装：
   - `PubSubClient`（Nick O'Leary）
   - `DHT sensor library`（Adafruit）
   - `Adafruit Unified Sensor`（上面那个的依赖，会自动装）
5. 打开 `esp8266_sensor_relay/esp8266_sensor_relay.ino`，改开头这几项：

```cpp
const char* WIFI_SSID     = "你的 WiFi 名";
const char* WIFI_PASSWORD = "你的 WiFi 密码";
const char* MQTT_HOST     = "192.168.1.100";  // 跑服务端的机器局域网 IP，不能填 127.0.0.1
const char* PRODUCT_KEY   = "factory_sensor"; // 与 devices 表一致
const char* DEVICE_SECRET = "secret_001";
const char* DEVICE_ID     = "dev_001";
```

6. 上传，打开串口监视器（**115200** baud）

> 查本机局域网 IP：macOS `ipconfig getifaddr en0`，Linux `hostname -I`

## 3. 验证

串口里应看到：

```
=== E2 IoT ESP8266 sensor/relay ===
[WiFi] connected, IP=192.168.1.23 rssi=-58 dBm
[MQTT] connecting as esp8266_dev_001 ...
[MQTT] connected
[MQTT] subscribed cmd/dev_001/exec
[DATA] devices/dev_001/data = 24.5°C / 61.0%
```

服务端日志应看到：

```
CONNECT cid=esp8266_dev_001 user=factory_sensor secret=secr...
PUBLISH from fd=.. topic=devices/dev_001/data payload_len=..
```

Qt 客户端「设备」页应能看到 `dev_001` 的最新数据在刷新。

### 下发指令

```bash
# 先登录取 token
TOKEN=$(curl -s -X POST http://127.0.0.1:8080/api/user \
  -H 'Content-Type: application/json' \
  -d '{"action":"login","username":"admin","password":"admin@123"}' | python3 -c 'import json,sys;print(json.load(sys.stdin)["token"])')

# 开继电器
curl -X POST http://127.0.0.1:8080/api/command \
  -H 'Content-Type: application/json' -H "Authorization: Bearer $TOKEN" \
  -d '{"device_id":"dev_001","cmd":"set_relay","payload":"on"}'
```

串口应打印 `[CMD] ... {"cmd":"set_relay","payload":"on"}` 和 `[RELAY] ON`。

设备离线时指令会进服务端队列，设备重连订阅成功后自动重放。

## 4. 常见问题

**`[MQTT] failed rc=-2`** — 连不上 broker。确认 `MQTT_HOST` 是服务端所在机器的**局域网 IP**、1883 端口没被防火墙拦、手机/电脑和 ESP8266 在同一网段。

**`rc=4` / `rc=5`** — 认证失败。核对 `PRODUCT_KEY` / `DEVICE_SECRET` 与 `devices` 表一致，且设备 `status` 为 `registered` 或 `active`：

```sql
SELECT product_key, device_id, device_secret, status FROM devices WHERE device_id='dev_001';
```

**串口一直 `DHT read failed`** — 接线/上拉电阻问题。DHT11 读取间隔要 ≥2 秒，这里 10 秒一次没问题。

**服务端日志报 `unsupported MQTT protocol level`** — 你的客户端库用的是 MQTT 5 或 3.1。服务端只支持 **3.1.1**，PubSubClient 默认就是 3.1.1，无需额外配置。

**设备重复上线 / 旧连接不释放** — 服务端在收到相同 `client_id` 的 CONNECT 时会主动踢掉旧连接，日志里能看到 `kick duplicate client_id=...`。所以 `client_id` 保持 `esp8266_<DEVICE_ID>` 这种稳定格式即可。
