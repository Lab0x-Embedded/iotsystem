/**
 * E2 IoT Platform — ESP8266 温湿度上报 + 继电器控制
 *
 * 硬件:  ESP8266 NodeMCU V3 / Wemos D1 mini
 * 传感器: DHT11 或 DHT22 (DATA → D2 / GPIO4)
 * 执行器: 继电器模块 (IN → D1 / GPIO5)
 *
 * 依赖库 (Arduino IDE → 库管理器搜索安装):
 *   - PubSubClient        (Nick O'Leary, 2.8+)
 *   - DHT sensor library  (Adafruit)
 *   - Adafruit Unified Sensor (Adafruit，DHT 库的依赖)
 * 板卡: 开发板管理器安装 "esp8266 by ESP8266 Community"，选 NodeMCU 1.0 (ESP-12E Module)
 *
 * 与服务端的约定:
 *   - MQTT: 3.1.1(level 4)  —— 服务端只支持 3.1.1
 *   - 认证: client_id = esp8266_<DEVICE_ID>, username = PRODUCT_KEY, password = DEVICE_SECRET
 *   - 上报: topic = devices/<DEVICE_ID>/data
 *           payload = {"device_id":"...","datapoints":[{"metric":"temperature","value":25.3,"ts":1696000000}]}
 *   - 指令: 订阅 cmd/<DEVICE_ID>/exec
 *           payload 可能是 {"cmd":"set_relay","payload":"on"} 或纯字符串 "on"/"off"/"reboot"
 */

#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>

// ─────────────────────────── 需要你修改的配置 ───────────────────────────
const char* WIFI_SSID     = "YOUR_WIFI_SSID";
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";

// 运行服务端的机器 IP（不是 127.0.0.1，要填局域网内实际 IP）
const char* MQTT_HOST     = "192.168.1.100";
const uint16_t MQTT_PORT  = 1883;

// 与数据库 devices 表一致（deploy/sql/init_data.sql）
const char* PRODUCT_KEY   = "factory_sensor";   // → devices.product_key
const char* DEVICE_SECRET = "secret_001";       // → devices.device_secret
const char* DEVICE_ID     = "dev_001";          // → devices.device_id
// ──────────────────────────────────────────────────────────────────────

// 引脚
#define DHT_PIN     4    // D2 (GPIO4)
#define DHT_TYPE    DHT11
#define RELAY_PIN   5    // D1 (GPIO5)
#define RELAY_ACTIVE_HIGH 1   // 低电平触发的继电器模块改成 0

// 上报间隔
const unsigned long REPORT_INTERVAL_MS = 10000UL;

DHT dht(DHT_PIN, DHT_TYPE);
WiFiClient wifiClient;
PubSubClient mqtt(wifiClient);

char topicData[64];
char topicCmd[64];
unsigned long lastReport = 0;

// ───────────────────────────── WiFi ─────────────────────────────
void setupWifi() {
    if (WiFi.status() == WL_CONNECTED) return;

    Serial.printf("[WiFi] connecting to %s", WIFI_SSID);
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
        if (millis() - start > 30000UL) {
            Serial.println("\n[WiFi] timeout, retry");
            WiFi.disconnect();
            delay(1000);
            WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
            start = millis();
        }
    }
    Serial.printf("\n[WiFi] connected, IP=%s rssi=%d dBm\n",
                  WiFi.localIP().toString().c_str(), WiFi.RSSI());
}

// ─────────────────────────── 指令处理 ───────────────────────────
void setRelay(bool on) {
    digitalWrite(RELAY_PIN, RELAY_ACTIVE_HIGH ? (on ? HIGH : LOW) : (on ? LOW : HIGH));
    Serial.printf("[RELAY] %s\n", on ? "ON" : "OFF");
}

// 从指令 payload 里判断动作：兼容 {"payload":"on"} 和纯字符串 "on"
bool payloadHas(const char* payload, unsigned int length, const char* needle) {
    if (!payload || !needle) return false;
    String s;
    s.reserve(length);
    for (unsigned int i = 0; i < length; i++) s += (char)tolower(payload[i]);
    return s.indexOf(needle) >= 0;
}

void onMqttMessage(char* topic, byte* payload, unsigned int length) {
    Serial.printf("[CMD] topic=%s len=%u payload=", topic, length);
    for (unsigned int i = 0; i < length; i++) Serial.print((char)payload[i]);
    Serial.println();

    if (payloadHas((const char*)payload, length, "on")) {
        setRelay(true);
    } else if (payloadHas((const char*)payload, length, "off")) {
        setRelay(false);
    } else if (payloadHas((const char*)payload, length, "reboot")) {
        Serial.println("[CMD] reboot requested");
        delay(200);
        ESP.restart();
    } else {
        Serial.println("[CMD] unrecognized, ignored");
    }
}

// ───────────────────────────── MQTT ─────────────────────────────
void mqttReconnect() {
    static unsigned long lastAttempt = 0;
    if (millis() - lastAttempt < 3000UL) return;   // 退避，避免打爆 broker
    lastAttempt = millis();

    char clientId[64];
    snprintf(clientId, sizeof(clientId), "esp8266_%s", DEVICE_ID);

    Serial.printf("[MQTT] connecting as %s ...\n", clientId);
    // username = product_key, password = device_secret
    bool ok = mqtt.connect(clientId, PRODUCT_KEY, DEVICE_SECRET);
    if (ok) {
        Serial.println("[MQTT] connected");
        if (mqtt.subscribe(topicCmd, 1))
            Serial.printf("[MQTT] subscribed %s\n", topicCmd);
        // 上线后主动推一次，让平台立刻有数据
        lastReport = 0;
    } else {
        Serial.printf("[MQTT] failed rc=%d (state=%d)\n", mqtt.state(), mqtt.state());
        // rc=4 认证失败 / rc=5 未授权 / rc=-2 连不上 broker
    }
}

// ─────────────────────────── 上报数据 ───────────────────────────
void reportOnce() {
    float humidity = dht.readHumidity();
    float tempC    = dht.readTemperature();

    if (isnan(humidity) || isnan(tempC)) {
        Serial.println("[DHT] read failed, skip this round");
        return;
    }

    // ts 用秒级时间戳；ESP8266 有 NTP 时可以换成真实时间
    unsigned long ts = millis() / 1000UL;

    char payload[256];
    snprintf(payload, sizeof(payload),
             "{\"device_id\":\"%s\",\"datapoints\":["
             "{\"metric\":\"temperature\",\"value\":%.1f,\"ts\":%lu},"
             "{\"metric\":\"humidity\",\"value\":%.1f,\"ts\":%lu}"
             "]}",
             DEVICE_ID, tempC, ts, humidity, ts);

    Serial.printf("[DATA] %s = %.1f°C / %.1f%%\n", topicData, tempC, humidity);
    mqtt.publish(topicData, payload, false);   // QoS 0（PubSubClient 的 publish 只支持 QoS0）
}

// ───────────────────────────── setup ─────────────────────────────
void setup() {
    Serial.begin(115200);
    Serial.println("\n\n=== E2 IoT ESP8266 sensor/relay ===");

    pinMode(RELAY_PIN, OUTPUT);
    setRelay(false);
    dht.begin();

    snprintf(topicData, sizeof(topicData), "devices/%s/data", DEVICE_ID);
    snprintf(topicCmd,  sizeof(topicCmd),  "cmd/%s/exec",      DEVICE_ID);

    setupWifi();

    mqtt.setServer(MQTT_HOST, MQTT_PORT);
    mqtt.setCallback(onMqttMessage);
    mqtt.setKeepAlive(30);        // 秒；服务端按 1.5× 判定超时
    mqtt.setSocketTimeout(5);     // 秒
    mqtt.setBufferSize(512);      // 默认 256，指令 payload 较长时不够
}

// ───────────────────────────── loop ─────────────────────────────
void loop() {
    if (WiFi.status() != WL_CONNECTED) {
        setupWifi();
    }

    if (!mqtt.connected()) {
        mqttReconnect();
    }
    mqtt.loop();   // 必须频繁调用，负责收指令 + 发心跳

    unsigned long now = millis();
    if (mqtt.connected() && now - lastReport >= REPORT_INTERVAL_MS) {
        lastReport = now;
        reportOnce();
    }
}
