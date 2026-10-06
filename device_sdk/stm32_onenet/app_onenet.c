#include "app_onenet.h"
#include "app_config.h"
#include "bsp_esp8266.h"

#include <stdio.h>
#include <string.h>

/* ============================================================
 *  通用层内部状态
 * ============================================================ */
static char g_product_id[32];
static char g_device_id[32];
static char g_auth_info[256];
static uint8_t g_mqtt_connected = 0;

/* 下行处理表（由业务层注册） */
static const OnenetHandler *g_handlers = NULL;
static size_t g_handler_count = 0;

/* 下行 payload 暂存：handler 里通常会调用 OneNET_Publish，而 Publish 会清
 * ESP 缓冲，所以必须先把报文拷出来再分派。 */
static char g_rx[ONENET_RX_MAX + 1];

/* 默认订阅列表（拼在 $sys/<pid>/<did> 后面）。
 * 需要命令下发时在这里加 "/cmd/request/+" 并同步在 OneNET 上配置。 */
static const char *const g_sub_suffixes[] = {
    "/thing/property/post/reply",
    "/thing/property/set",
    /* 服务下行 topic 是 …/thing/service/<identifier>/invoke（实测）。
     * 平台是**按订阅通配匹配**下发目标的，所以必须订到 /invoke 这一层：
     * 只订 "/thing/service/+" 或 "/thing/service/reboot" 都会报 10415 dev not subscribed
     * （+ 只吃一层，少一层 /invoke）。用 +/invoke 一次覆盖所有服务，新增服务不用改这里。*/
    "/thing/service/+/invoke",
};

/* ============================================================
 *  调试输出（APP_ONENET_DEBUG=1 才编进来）
 * ============================================================ */
#if APP_ONENET_DEBUG
static void dump_esp(const char *tag) {
  printf("[ONENET] ==== ESP raw (%s) ====\r\n%s\r\n"
         "[ONENET] ========================\r\n",
         tag, BSP_ESP8266_GetBuffer());
}
#else
#define dump_esp(tag) ((void)0)
#endif

/* ============================================================
 *              BSP 返回值 → ONENET 状态码
 * ============================================================ */
static ONENET_Status_t map_esp_result(uint8_t esp_ret) {
  switch (esp_ret) {
  case ESP_OK:
    return ONENET_OK;
  case ESP_ERR_FAIL:
    return ONENET_ERR_AT;
  case ESP_ERR_TIMEOUT:
    return ONENET_ERR_TIMEOUT;
  default:
    return ONENET_ERR_INIT;
  }
}

/* ============================================================
 *                     topic / 报文工具
 * ============================================================ */
static void build_topic(char *dst, size_t n, const char *suffix) {
  snprintf(dst, n, "$sys/%s/%s%s", g_product_id, g_device_id, suffix);
}

/* topic 尾部是否等于 suffix（精确匹配，不做子串猜测） */
static uint8_t topic_match(const char *topic, const char *suffix) {
  size_t tl = strlen(topic);
  size_t sl = strlen(suffix);

  if (tl < sl)
    return 0;
  return strcmp(topic + tl - sl, suffix) == 0;
}

/* 解析十进制正整数，返回消耗的字符数（0 = 解析失败） */
static int parse_uint(const char *s, int *out) {
  int i = 0;
  unsigned v = 0;

  while (s[i] >= '0' && s[i] <= '9' && i < 9) {
    v = v * 10u + (unsigned)(s[i] - '0');
    i++;
  }
  if (i == 0)
    return 0;
  *out = (int)v;
  return i;
}

/* 按处理表分派。返回 1 = 有处理器命中 */
static uint8_t dispatch(const char *topic, const char *payload, size_t len) {
  size_t i;

  for (i = 0; i < g_handler_count; i++) {
    uint8_t hit;

    if (g_handlers[i].handler == NULL)
      continue;

    if (g_handlers[i].match_anywhere)
      hit = (strstr(topic, g_handlers[i].topic_suffix) != NULL);
    else
      hit = topic_match(topic, g_handlers[i].topic_suffix);

    if (hit) {
      g_handlers[i].handler(topic, payload, len);
      return 1;
    }
  }
  printf("[ONENET] 未处理的下行 topic=%s payload=%s\r\n", topic, payload);
  return 0;
}

/* ============================================================
 *                       会话
 * ============================================================ */
ONENET_Status_t OneNET_MqttInit(const char *product_id, const char *device_id,
                                const char *token) {
  char cmd[512];
  uint8_t ret;

  if (product_id == NULL || device_id == NULL || token == NULL)
    return ONENET_ERR_INIT;

  snprintf(g_product_id, sizeof g_product_id, "%s", product_id);
  snprintf(g_device_id, sizeof g_device_id, "%s", device_id);

  /* Token 长度显式检查：截断后的 Token 会以 CONNACK 拒绝的形式报错，很难查 */
  if (snprintf(g_auth_info, sizeof g_auth_info, "%s", token) >=
      (int)sizeof g_auth_info) {
    printf("[ONENET] Token 太长：%u 字节，上限 %u\r\n", (unsigned)strlen(token),
           (unsigned)sizeof g_auth_info - 1);
    return ONENET_ERR_INIT;
  }

  g_mqtt_connected = 0;
  printf("[ONENET] PID=%s DID=%s Token=%u 字节\r\n", g_product_id, g_device_id,
         (unsigned)strlen(g_auth_info));

  /* 清旧连接 / 清状态机：解决"每次要断电 ESP8266"。
   * 注意：这些 AT 与 WiFi 无关的前提是 WIFI_Init/WIFI_Connect 已经跑过。 */
  /* ---- AT 通道 + 固件版本自检（诊断用，可删）----
     全 ERROR 的时候先确认：AT 本身通不通、MQTT 命令集/长度上限由固件版本决定 */
  {
    uint8_t r_at = BSP_ESP8266_SendAT_Wait("AT", "OK", 1000);
    printf("[ONENET] AT ret=%d\r\n", r_at);
    dump_esp("AT");
    BSP_ESP8266_SendAT_Wait("AT+GMR", "OK", 2000);
    printf("[ONENET] AT+GMR（AT 固件版本）:\r\n");
    dump_esp("GMR");
  }

  ret = BSP_ESP8266_SendAT_Wait("AT+MQTTDISCONN=0", "OK", 2000);
  printf("[ONENET] MQTTDISCONN ret=%d\r\n", ret);
  dump_esp("MQTTDISCONN");
  HAL_Delay(200);

  ret = BSP_ESP8266_SendAT_Wait("AT+MQTTCLEAN=0", "OK", 2000);
  printf("[ONENET] MQTTCLEAN ret=%d\r\n", ret);
  dump_esp("MQTTCLEAN");
  HAL_Delay(200);

  snprintf(cmd, sizeof cmd, "AT+MQTTUSERCFG=0,1,\"%s\",\"%s\",\"%s\",0,0,\"\"",
           g_device_id, g_product_id, g_auth_info);
  ret = BSP_ESP8266_SendAT_Wait(cmd, "OK", 5000);
  printf("[ONENET] MQTTUSERCFG ret=%d\r\n", ret);
  dump_esp("MQTTUSERCFG");

  if (ret != ESP_OK) {
    /* ---- 二分自检（诊断用，可删）----
       用"短密码"再发一次：成功 => 是 Token(长度/字符) 的问题；
       仍 ERROR => 模块 MQTT 状态坏了（断电/AT+RST 才能恢复），跟 Token 无关。 */
    uint8_t r2;
    printf("[ONENET] 诊断：用短密码 \"1\" 再试一次 USERCFG\r\n");
    BSP_ESP8266_SendAT_Wait("AT+MQTTCLEAN=0", "OK", 2000);
    r2 = BSP_ESP8266_SendAT_Wait(
        "AT+MQTTUSERCFG=0,1,\"test\",\"test\",\"1\",0,0,\"\"", "OK", 3000);
    printf("[ONENET] USERCFG(短密码) ret=%d  => %s\r\n", r2,
           (r2 == ESP_OK) ? "模块命令集正常，问题在 Token（长度/字符）"
                          : "模块 MQTT 状态卡死，需 AT+RST 或给模块断电重启");
    dump_esp("USERCFG(short)");
    BSP_ESP8266_SendAT_Wait("AT+MQTTCLEAN=0", "OK", 2000); /* 清掉测试 client */
    return map_esp_result(ret);
  }

  return ONENET_OK;
}

ONENET_Status_t OneNET_MqttConnect(void) {
  char cmd[256];
  char topic[160];
  uint8_t ret;
  uint32_t t0, rx0;
  size_t i;

  printf("[ONENET] ========== Connect Start ==========\r\n");

  /* 最后的 reconnect 参数用 0（关掉模块自动重连，重连由我们决定） */
  snprintf(cmd, sizeof cmd, "AT+MQTTCONN=0,\"%s\",%d,0", ONENET_MQTT_SERVER,
           ONENET_MQTT_PORT);

  t0 = HAL_GetTick();
  rx0 = BSP_ESP8266_RxCount();
  ret = BSP_ESP8266_SendAT_Wait(cmd, "+MQTTCONNECTED", 60000);
  printf("[ONENET] MQTTCONN ret=%d %lums rx=%lu(+%lu) drop=%lu err=%lu\r\n", ret,
         (unsigned long)(HAL_GetTick() - t0), (unsigned long)BSP_ESP8266_RxCount(),
         (unsigned long)(BSP_ESP8266_RxCount() - rx0),
         (unsigned long)BSP_ESP8266_DropCount(),
         (unsigned long)BSP_ESP8266_ErrCount());
  dump_esp("MQTTCONN");

  if (ret != ESP_OK)
    return map_esp_result(ret);

  g_mqtt_connected = 1;

  /* 订阅：单项失败不致命（属性上报仍然能用），但会把原因打出来 */
  for (i = 0; i < sizeof g_sub_suffixes / sizeof g_sub_suffixes[0]; i++) {
    build_topic(topic, sizeof topic, g_sub_suffixes[i]);
    snprintf(cmd, sizeof cmd, "AT+MQTTSUB=0,\"%s\",1", topic);
    ret = BSP_ESP8266_SendAT_Wait(cmd, "OK", 5000);
    printf("[ONENET] SUB %s ret=%d\r\n", g_sub_suffixes[i], ret);
    dump_esp("MQTTSUB");
    if (ret != ESP_OK) {
      printf("[ONENET] 订阅失败：%s（下发该功能不可用）\r\n", g_sub_suffixes[i]);
    }
  }

  printf("[ONENET] ========== Connect OK ==========\r\n");
  return ONENET_OK;
}

void OneNET_Disconnect(void) {
  printf("[ONENET] Disconnecting...\r\n");
  BSP_ESP8266_SendAT_Wait("AT+MQTTDISCONN=0", "OK", 3000);
  g_mqtt_connected = 0;
  printf("[ONENET] Disconnected\r\n");
}

uint8_t OneNET_IsConnected(void) { return g_mqtt_connected; }

void OneNET_SetHandlers(const OnenetHandler *table, size_t count) {
  g_handlers = table;
  g_handler_count = (table == NULL) ? 0 : count;
}

/* ============================================================
 *                       发布
 * ============================================================ */
ONENET_Status_t OneNET_Publish(const char *topic_suffix, const char *payload) {
  char topic[160];

  if (topic_suffix == NULL)
    return ONENET_ERR_SEND;

  build_topic(topic, sizeof topic, topic_suffix);
  return OneNET_PublishAbsolute(topic, payload);
}

/* topic 已经拼好（服务应答这种 topic 带变量的场景） */
ONENET_Status_t OneNET_PublishAbsolute(const char *topic, const char *payload) {
  char cmd[512];
  int plen, need;
  uint8_t ret;

  if (!g_mqtt_connected)
    return ONENET_ERR_NOT_CONNECTED;
  if (topic == NULL || payload == NULL)
    return ONENET_ERR_SEND;

  plen = (int)strlen(payload);

  /* 先算真实长度再拼：以前固定放行 512 字节，snprintf 会把 AT 命令悄悄截断，
   * 而报文里声明的长度还是原值 → 上报失败且极难查。 */
  need = snprintf(NULL, 0, "AT+MQTTPUBRAW=0,\"%s\",%d,0,0\r\n", topic, plen);
  if (need < 0 || need + plen + 1 > (int)sizeof cmd) {
    printf("[ONENET] 发布内容过长：topic=%u payload=%d（上限 %u）\r\n",
           (unsigned)strlen(topic), plen, (unsigned)sizeof cmd);
    return ONENET_ERR_SEND;
  }

  snprintf(cmd, sizeof cmd, "AT+MQTTPUBRAW=0,\"%s\",%d,0,0", topic, plen);

#if APP_ONENET_DEBUG
  printf("[ONENET] pub topic=%s payload=%s\r\n", topic, payload);
#endif

  /* 握手 + 送数据 + 等 ack 这一套在 BSP 里，业务层不再重复 */
  ret = BSP_ESP8266_SendAT_WaitThenData(cmd, ">", (const uint8_t *)payload,
                                        (uint16_t)plen, "OK", 3000, 5000);
  if (ret != ESP_OK) {
    /* 失败路径始终打印原文：ret=1 超时 / ret=2 模块回了 ERROR 或 FAIL */
    printf("[ONENET] 发布失败 ret=%d payload=%s\r\n[ONENET] ESP raw: %s\r\n",
           ret, payload, BSP_ESP8266_GetBuffer());
    return ONENET_ERR_SEND;
  }

  return ONENET_OK;
}

ONENET_Status_t OneNET_PublishProperty(const char *params_json) {
  char payload[320];
  int n;

  if (params_json == NULL)
    return ONENET_ERR_SEND;

  /* 协议信封（id/params）由通用层拼；params 的内容由业务层给 */
  n = snprintf(payload, sizeof payload, "{\"id\":\"%lu\",\"params\":%s}",
               (unsigned long)HAL_GetTick(), params_json);
  if (n <= 0 || n >= (int)sizeof payload) {
    printf("[ONENET] 上报报文过长（%d 字节，上限 %u）\r\n", n,
           (unsigned)sizeof payload);
    return ONENET_ERR_SEND;
  }

  return OneNET_Publish("/thing/property/post", payload);
}

/* ============================================================
 *           主循环轮询：解析 +MQTTSUBRECV 下行
 *
 *  报文格式：+MQTTSUBRECV:0,"<topic>",<len>,<payload>\r\n
 *  按报文里声明的 <len> 取数据（不再按 '\n' 拆行、也没有 256 字节行上限）
 * ============================================================ */
uint8_t OneNET_Poll(void) {
  uint8_t handled = 0;

  for (;;) {
    char *acc = BSP_ESP8266_GetBuffer(); /* 内部先 pump 环形缓冲 */
    uint16_t avail = BSP_ESP8266_GetLength();
    char *sub, *dis;

    if (avail == 0)
      break;

    sub = strstr(acc, "+MQTTSUBRECV");
    dis = strstr(acc, "+MQTTDISCONNECTED");
    if (!sub && !dis)
      break;

    /* 掉线事件优先：清连接状态，否则后面 publish 会一直"假成功" */
    if (dis && (!sub || dis < sub)) {
      printf("[ONENET] 收到 +MQTTDISCONNECTED，连接状态清零\r\n");
      g_mqtt_connected = 0;
      char *nl = strchr(dis, '\n');
      BSP_ESP8266_Consume(nl ? (uint16_t)(nl - acc + 1) : avail);
      continue;
    }

    /* 报文之前的垃圾字节先丢掉，下一轮从报文头开始解析 */
    if (sub != acc) {
      BSP_ESP8266_Consume((uint16_t)(sub - acc));
      continue;
    }

    /* 解析 topic */
    char *q1 = strchr(acc, '"');
    char *q2 = q1 ? strchr(q1 + 1, '"') : NULL;
    if (q1 == NULL || q2 == NULL)
      break; /* 还没收全 */

    /* 解析声明的长度 */
    char *c1 = strchr(q2 + 1, ',');
    int plen = 0, used;
    if (c1 == NULL || (used = parse_uint(c1 + 1, &plen)) == 0)
      break; /* 还没收全 */

    /* payload 起点 = 长度字段后的第一个逗号之后 */
    char *c2 = strchr(c1 + 1 + used, ',');
    if (c2 == NULL)
      break;
    char *payload = c2 + 1;
    size_t off = (size_t)(payload - acc);

    /* 报文结束标志必须是 payload 之后第一个 "\r\n"：
     *   - 还没有 CRLF        -> 行没收全，等下一轮
     *   - CRLF 不在声明长度处 -> 声明长度不对，跳过这一行（否则会一直卡住）
     *   - CRLF 正好在 payload+len -> 正常 */
    char *term = NULL;
    size_t i;
    for (i = off; i + 1 < (size_t)avail; i++) {
      if (acc[i] == '\r' && acc[i + 1] == '\n') {
        term = acc + i;
        break;
      }
    }
    if (term == NULL)
      break;

    if (term != payload + plen) {
      printf("[ONENET] 下行长度不符（声明 %d，实际 %d），跳过该段\r\n", plen,
             (int)(term - payload));
      BSP_ESP8266_Consume((uint16_t)((size_t)(term - acc) + 2));
      continue;
    }

    /* topic 拷到局部（payload 之后可能触发 Publish → 清缓冲） */
    char topic[160];
    size_t tlen = (size_t)(q2 - q1 - 1);
    if (tlen >= sizeof topic)
      tlen = sizeof topic - 1;
    memcpy(topic, q1 + 1, tlen);
    topic[tlen] = '\0';

    /* 先消费掉这一段，再分派：handler 里发回复时会清 ESP 缓冲 */
    BSP_ESP8266_Consume((uint16_t)(off + (size_t)plen + 2));

    if ((size_t)plen > ONENET_RX_MAX) {
      printf("[ONENET] 下行 %d 字节 > 上限 %d，丢弃\r\n", plen, ONENET_RX_MAX);
      continue;
    }

    memcpy(g_rx, payload, (size_t)plen);
    g_rx[plen] = '\0';
    if (dispatch(topic, g_rx, (size_t)plen))
      handled = 1;
  }

  return handled;
}
