/**
 * @file esp8266_at_mqtt.c
 *
 * ESP8266-01S（ESP-AT v2.2）MQTT AT 指令参考驱动实现。
 *
 * 接收路径是流式解析（不是逐行读）：
 *  - 普通应答/URC 按 \r\n 切行分发；
 *  - 唯一例外是 +MQTTSUBRECV：payload 为原样字节、可能内含 \r\n，
 *    必须先解析出头部的字节数声明，再从流里按字节取满，逐行处理会丢数据。
 *  - expect() 与 esp_at_poll() 共用同一个流解析器，避免两处读串口互相吞字节。
 */
#include "esp8266_at_mqtt.h"

#include <stdio.h>
#include <string.h>

/* ── 内部状态 ─────────────────────────────────────────────── */

#define LINE_CAP      192      /* 普通行上限 */
#define RBUF_CAP      1024     /* 滚动接收缓冲 */
#define PAYLOAD_CAP   256      /* 指令 payload 交付上限（超出部分丢弃，字节数仍统计） */
#define POLL_MS_STEP  10       /* at_uart_read 单次建议的超时粒度 */

static esp_at_config_t  g_cfg;
static esp_at_cmd_cb_t  g_cmd_cb;
static int              g_mqtt_up;      /* +MQTTCONNECTED / +MQTTDISCONNECTED */

static char rbuf[RBUF_CAP];
static int  rlen = 0;

/* SUBRECV 半途状态：头部已解析、payload 未取满 */
static struct {
    int      active;
    char     topic[128];
    int      topic_len;
    int      payload_len;
    char     payload[PAYLOAD_CAP];
    int      got;
} subrecv;

/* expect 的等待目标 */
static const char *s_want;          /* NULL = poll 模式 */
static int         s_want_hit;      /* 命中 want / ERROR */

/* ── 小工具 ───────────────────────────────────────────────── */

static void msleep(int ms)
{
    /* 移植方按平台替换：STM32 用 HAL_Delay / 51 用定时器或空循环 */
    volatile int i;
    for (; ms > 0; ms -= POLL_MS_STEP)
        for (i = 0; i < 8000; i++) { /* 粗略延时，仅参考实现 */ }
}

static int starts_with(const char *s, int slen, const char *prefix)
{
    size_t n = strlen(prefix);
    return slen >= (int)n && strncmp(s, prefix, n) == 0;
}

/* 在 buf 前 blen 字节里找子串，返回偏移；找不到返回 -1 */
static int find_sub(const char *buf, int blen, const char *sub)
{
    int n = (int)strlen(sub);
    if (n == 0 || blen < n) return -1;
    for (int i = 0; i <= blen - n; i++)
        if (buf[i] == sub[0] && memcmp(buf + i, sub, n) == 0) return i;
    return -1;
}

static void rbuf_shift(int consumed)
{
    if (consumed <= 0) return;
    rlen -= consumed;
    if (rlen > 0) memmove(rbuf, rbuf + consumed, rlen);
}

static void rbuf_fill(void)
{
    int n = at_uart_read(rbuf + rlen, RBUF_CAP - rlen);
    if (n > 0) rlen += n;
}

/* ── 行 / URC 分发 ────────────────────────────────────────── */

static void dispatch_line(const char *line)
{
    int len = (int)strlen(line);

    /* 状态维护与 expect 匹配相互独立：等待目标可能同时是状态行
     * （如 +MQTTCONNECTED:0），不能在这里提前 return。 */
    if (starts_with(line, len, "+MQTTCONNECTED:0"))    g_mqtt_up = 1;
    else if (starts_with(line, len, "+MQTTDISCONNECTED:0")) g_mqtt_up = 0;

    if (s_want) {
        if (starts_with(line, len, s_want)) s_want_hit = 1;
        else if (strcmp(line, "ERROR") == 0) s_want_hit = -1;
    }
}

/* 尝试解析 rbuf 里位于 off 的 +MQTTSUBRECV 头部。
 * @param hdr_len_out 输出头部长度（payload 从 off + hdr_len_out 开始）
 * @return 0 已解析出完整头部；1 头部不完整需要更多数据；-1 格式错 */
static int parse_subrecv_header(int off, int *hdr_len_out)
{
    const char *p = rbuf + off;
    const char *end = rbuf + rlen;

    /* "+MQTTSUBRECV:0,"topic",len," */
    if (end - p < 15) return 1;                   /* 前缀 + LinkID 都没齐 */
    p += strlen("+MQTTSUBRECV:");
    if (*p++ != '0' || *p++ != ',') return -1;
    if (*p++ != '"') return -1;

    subrecv.topic_len = 0;
    while (p < end && *p != '"' && subrecv.topic_len < (int)sizeof(subrecv.topic) - 1)
        subrecv.topic[subrecv.topic_len++] = *p++;
    if (p >= end) return 1;                       /* topic 未收齐 */
    if (subrecv.topic_len >= (int)sizeof(subrecv.topic) - 1) return -1;
    if (*p++ != '"' || *p++ != ',') return -1;

    subrecv.payload_len = 0;
    while (p < end && *p >= '0' && *p <= '9')
        subrecv.payload_len = subrecv.payload_len * 10 + (*p++ - '0');
    if (p >= end) return 1;                       /* 长度数字未收齐 */
    if (*p++ != ',') return -1;

    if (hdr_len_out) *hdr_len_out = (int)(p - (rbuf + off));
    return 0;
}

static void finish_subrecv(void)
{
    if (g_cmd_cb)
        g_cmd_cb(subrecv.topic, subrecv.topic_len,
                 subrecv.payload, subrecv.got < subrecv.payload_len
                                  ? subrecv.got : subrecv.payload_len);
    memset(&subrecv, 0, sizeof(subrecv));
    s_want_hit = 1;                               /* 收到指令也算一次事件 */
}

/* 处理接收流。返回 1 = 本轮有值得注意的事件（指令交付 / want 命中）。 */
static int process_stream(void)
{
    int event = 0;

    for (;;) {
        /* 1) SUBRECV payload 读取中 */
        if (subrecv.active) {
            int remaining = subrecv.payload_len - subrecv.got;
            int take = remaining < rlen ? remaining : rlen;
            if (take > 0) {
                int copy = take < (int)sizeof(subrecv.payload) - subrecv.got
                           ? take : (int)sizeof(subrecv.payload) - subrecv.got;
                if (copy > 0) memcpy(subrecv.payload + subrecv.got, rbuf, copy);
                subrecv.got += take;              /* got 计真实字节数，超出 CAP 的丢弃 */
                rbuf_shift(take);
            }
            if (subrecv.got >= subrecv.payload_len) {
                finish_subrecv();
                event = 1;
            }
            if (subrecv.active) return event;     /* 还差 payload，等更多数据 */
            continue;
        }

        /* 2) 缓冲里找 +MQTTSUBRECV（URC 前缀之前可能还有普通行/碎片） */
        int s = find_sub(rbuf, rlen, "+MQTTSUBRECV:");
        if (s >= 0) {
            /* 先把前缀之前的普通行处理掉 */
            int nl = find_sub(rbuf, s, "\n");
            if (nl >= 0) {
                int e = nl;
                if (e > 0 && rbuf[e-1] == '\r') e--;
                rbuf[e] = '\0';
                dispatch_line(rbuf);
                rbuf_shift(nl + 1);
                event = event || s_want_hit;
                continue;
            }
            /* 前缀前没有完整行：若前缀前有残余字节且不是空行，先丢掉
             * （正常流里 SUBRECV 应出现在行首） */
            if (s > 0) rbuf_shift(s);

            int hdr_len = 0;
            int hrc = parse_subrecv_header(0, &hdr_len);
            if (hrc == 1) return event;           /* 头部不完整，等更多数据 */
            if (hrc == -1) {                      /* 格式错：丢一个字节再找 */
                rbuf_shift(1);
                continue;
            }
            subrecv.active = 1;
            rbuf_shift(hdr_len);                  /* 消费头部，后面是 payload 字节 */
            continue;
        }

        /* 3) 没有 SUBRECV：按普通行处理 */
        int nl = find_sub(rbuf, rlen, "\n");
        if (nl >= 0) {
            int e = nl;
            if (e > 0 && rbuf[e-1] == '\r') e--;
            rbuf[e] = '\0';
            dispatch_line(rbuf);
            rbuf_shift(nl + 1);
            event = event || s_want_hit;
            continue;
        }

        /* 行未收齐 */
        if (rlen >= RBUF_CAP) rbuf_shift(rlen / 2); /* 防溢出：丢半截 */
        return event;
    }
}

/* ── 对外接口 ─────────────────────────────────────────────── */

void esp_at_set_cmd_cb(esp_at_cmd_cb_t cb) { g_cmd_cb = cb; }

int esp_at_connected(void) { return g_mqtt_up; }

/* 发指令并等指定应答；cmd 为 NULL 表示只等（MQTTPUBRAW payload 之后） */
static int expect(const char *cmd, const char *want, int timeout_ms)
{
    int waited = 0;

    if (cmd) {
        int sent = 0, total = (int)strlen(cmd);
        while (sent < total) {
            int n = at_uart_write(cmd + sent, total - sent);
            if (n <= 0) return -1;
            sent += n;
        }
        if (at_uart_write("\r\n", 2) != 2) return -1;
    }

    s_want = want; s_want_hit = 0;
    while (s_want_hit == 0) {
        rbuf_fill();
        if (process_stream()) break;              /* SUBRECV 交付即视为响应 */
        if (s_want_hit != 0) break;
        if (waited >= timeout_ms) { s_want = 0; return -1; }
        waited += POLL_MS_STEP;
        msleep(POLL_MS_STEP);
    }
    {
        int rc = (s_want_hit == 1) ? 0 : -1;
        s_want = 0;
        return rc;
    }
}

int esp_at_init(const esp_at_config_t *cfg)
{
    char cmd[256];
    /* 平台约定：订阅的是 cmd/<设备ID>/exec。
     * 设备 ID = client_id 去掉可选的 "esp8266_" 前缀。 */
    const char *dev_id = cfg->client_id;

    if (strncmp(dev_id, "esp8266_", 8) == 0) dev_id += 8;

    g_cfg = *cfg;
    g_mqtt_up = 0;
    rlen = 0;
    memset(&subrecv, 0, sizeof(subrecv));

    /* 1. 模块握手（上电慢，多试两次） */
    msleep(500);
    if (expect("AT", "OK", 1000) != 0 &&
        expect("AT", "OK", 1000) != 0 &&
        expect("AT", "OK", 1000) != 0) return -1;
    expect("ATE0", "OK", 1000);                   /* 关回显，简化解析 */

    /* 2. WiFi */
    expect("AT+CWMODE=1", "OK", 2000);
    snprintf(cmd, sizeof(cmd), "AT+CWJAP=\"%s\",\"%s\"",
             cfg->wifi_ssid, cfg->wifi_pass);
    if (expect(cmd, "WIFI GOT IP", 20000) != 0) return -2;

    /* 3. MQTT 配置 + 保活 + 连接（ERROR 多半是固件版本旧，见 README 第 2 节） */
    snprintf(cmd, sizeof(cmd), "AT+MQTTUSERCFG=0,1,\"%s\",\"%s\",\"%s\",0,0,\"\"",
             cfg->client_id, cfg->username, cfg->password);
    if (expect(cmd, "OK", 3000) != 0) return -3;

    snprintf(cmd, sizeof(cmd), "AT+MQTTCONNCFG=0,%d,1,\"\",\"\",0,0",
             cfg->keepalive_sec);
    if (expect(cmd, "OK", 3000) != 0) return -3;

    snprintf(cmd, sizeof(cmd), "AT+MQTTCONN=0,\"%s\",%d,1",
             cfg->mqtt_host, cfg->mqtt_port);
    if (expect(cmd, "+MQTTCONNECTED:0", 15000) != 0) return -3;

    /* 4. 订阅平台指令 topic */
    snprintf(cmd, sizeof(cmd), "AT+MQTTSUB=0,\"cmd/%s/exec\",1", dev_id);
    if (expect(cmd, "OK", 5000) != 0) return -3;

    return 0;
}

int esp_at_publish(const char *topic, const char *json, int len)
{
    char cmd[160];

    if (!g_mqtt_up) return -1;

    snprintf(cmd, sizeof(cmd), "AT+MQTTPUBRAW=0,\"%s\",%d,1,0", topic, len);
    if (expect(cmd, ">", 5000) != 0) return -1;   /* 等 > 提示符 */

    /* 原样发 payload 字节（无转义、无行尾） */
    {
        int sent = 0;
        while (sent < len) {
            int n = at_uart_write(json + sent, len - sent);
            if (n <= 0) return -1;
            sent += n;
        }
    }
    return expect(NULL, "+MQTTPUB:OK", 5000);
}

int esp_at_poll(int timeout_ms)
{
    int waited = 0, event = 0;

    s_want = 0; s_want_hit = 0;
    for (;;) {
        rbuf_fill();
        if (process_stream()) { event = 1; }
        if (subrecv.active) {                     /* payload 未收齐，继续等 */
            if (waited >= timeout_ms) break;
            waited += POLL_MS_STEP;
            msleep(POLL_MS_STEP);
            continue;
        }
        if (event || waited >= timeout_ms) break;
        waited += POLL_MS_STEP;
        msleep(POLL_MS_STEP);
    }
    return event;
}
