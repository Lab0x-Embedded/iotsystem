#include "app_device.h"

#include "app_config.h"
#include "app_onenet.h"
#include "board_pins.h"
#include "beep.h"
#include "bsp_dht11.h"
#include "cJSON.h"
#include "fan.h"
#include "led.h"

#include <stdio.h>
#include <string.h>

/* ============================================================
 *  可控对象表：物模型标识符 → 板上的执行器
 *
 *  - 标识符必须与 OneNET 物模型里的一致（led1/led2/led3 是小写！），
 *    改物模型只改这张表
 *  - 取值 bool(true/false) 与 string("on"/"off") 都认，
 *    所以同一种开关在物模型里用哪种类型都不影响
 *  - 灯 1/2/3 分别对应 BSP/Src/led.c 的 LED0/LED1/LED2
 *    （引脚真值在 BSP/Inc/board_pins.h）
 *
 * ============================================================ */
enum { ACT_LED = 0, ACT_BEEP, ACT_FAN };

typedef struct {
  const char *id;    /* 物模型标识符（大小写必须与平台一致）*/
  uint8_t kind;      /* ACT_xxx */
  uint8_t index;     /* 灯用：led.c 里的编号 */
  uint8_t str_value; /* 1 = 该属性是字符串型，上报用 "on"/"off"；
                        0 = 布尔型，上报用 true/false */
  uint8_t state;     /* 当前状态 */
  uint8_t seen;      /* 是否收到过该属性的下发（= 物模型里确实存在）*/
} Actuator_t;

/* 与 OneNET 物模型一一对应（2026-10-04 实测）：
 *   led1 / led2 / led3  string(8)   ← "on" / "off"
 *   buzzer / fan        bool        ← true / false
 * 只在物模型里改了标识符或类型，才需要动这张表。*/
static Actuator_t s_actuators[] = {
    {.id = "led1", .kind = ACT_LED, .index = 0, .str_value = 1},
    {.id = "led2", .kind = ACT_LED, .index = 1, .str_value = 1},
    {.id = "led3", .kind = ACT_LED, .index = 2, .str_value = 1},
    {.id = "buzzer", .kind = ACT_BEEP, .str_value = 0},
    {.id = "fan", .kind = ACT_FAN, .str_value = 0},
};
#define ACTUATOR_COUNT (sizeof s_actuators / sizeof s_actuators[0])

/* ============================================================
 *  工具
 * ============================================================ */

/* 把云端来的 id 限制成 [0-9A-Za-z_-]，其它字符丢掉；空则用 "0"。
 * 直接把任意内容 snprintf 进 JSON 会被引号/反斜杠破坏整个回复报文。 */
static void sanitize_id(const char *src, char *dst, size_t n) {
  size_t k = 0;

  if (n == 0)
    return;

  if (src != NULL) {
    for (; *src != '\0' && k + 1 < n; src++) {
      char c = *src;
      if ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') ||
          (c >= 'a' && c <= 'z') || c == '_' || c == '-')
        dst[k++] = c;
    }
  }
  if (k == 0)
    dst[k++] = '0';
  dst[k] = '\0';
}

static uint8_t str_is_on(const char *v) {
  return v && (!strcmp(v, "on") || !strcmp(v, "1") || !strcmp(v, "open") ||
               !strcmp(v, "true"));
}

static uint8_t str_is_off(const char *v) {
  return v && (!strcmp(v, "off") || !strcmp(v, "0") || !strcmp(v, "close") ||
               !strcmp(v, "false"));
}

/* 属性节点 → 开关值。返回 0 = 类型/取值不认识 */
static uint8_t parse_switch(const cJSON *item, uint8_t *on) {
  if (cJSON_IsBool(item)) {
    *on = cJSON_IsTrue(item) ? 1 : 0;
    return 1;
  }
  if (cJSON_IsNumber(item)) {
    *on = (item->valuedouble != 0) ? 1 : 0;
    return 1;
  }
  if (cJSON_IsString(item) && item->valuestring) {
    if (str_is_on(item->valuestring)) {
      *on = 1;
      return 1;
    }
    if (str_is_off(item->valuestring)) {
      *on = 0;
      return 1;
    }
    printf("[DEV] %s 收到未知取值：%s\r\n",
           item->string ? item->string : "?", item->valuestring);
  }
  return 0;
}

/* 读回硬件当前状态（上电时用它填初值，这样上报的是真值而不是猜 0）*/
static uint8_t actuator_read(const Actuator_t *a) {
  switch (a->kind) {
  case ACT_LED:
    return LED_IsOn(a->index);
  case ACT_BEEP:
    return BEEP_IsOn();
  case ACT_FAN:
    return FAN_IsOn();
  default:
    return 0;
  }
}

/* 真正驱动硬件 */
static void actuator_apply(Actuator_t *a, uint8_t on) {
  switch (a->kind) {
  case ACT_LED:
    LED_Set(a->index, on);
    break;
  case ACT_BEEP:
    BEEP_Set(on);
    break;
  case ACT_FAN:
    FAN_Set(on);
    break;
  default:
    return;
  }
  a->state = on;
  printf("[DEV] %s = %u\r\n", a->id, (unsigned)on);
}

/* 属性设置回执（id 用清洗过的，code/msg 固定） */
static void reply_property_set(const char *id) {
  char resp[160];

  snprintf(resp, sizeof resp, "{\"id\":\"%s\",\"code\":200,\"msg\":\"success\"}",
           id);
  if (OneNET_Publish("/thing/property/set_reply", resp) != ONENET_OK)
    printf("[DEV] set_reply 发送失败\r\n");
}

/* 把已下发过的执行器状态回报给平台。
 * 只报 seen=1 的项：收到过下发就说明物模型里确实有该属性，上报不会被平台拒；
 * 平台侧属性值 = 设备真实状态，面板轮询才能显示对。
 * 物模型确认齐全后可把 APP_REPORT_ACTUATOR_STATE 置 1 强制全量上报。*/
/* 把一个执行器追加成 "id":{"value":x}（first=0 时带前导逗号）。
 * 返回追加后的长度；装不下就原样返回。
 * 必须带 {"value":x} 包装：平铺写法平台会回
 * {"code":2402,"msg":"request format error"}（实测）。*/
static size_t append_actuator(char *buf, size_t cap, size_t k, uint8_t *first,
                             const Actuator_t *a) {
  int n = snprintf(buf + k, cap - k, "%s\"%s\":{\"value\":%s}", *first ? "" : ",",
                   a->id, a->str_value ? (a->state ? "\"on\"" : "\"off\"")
                                       : (a->state ? "true" : "false"));
  if (n <= 0 || (size_t)n >= cap - k)
    return k;
  *first = 0;
  return k + (size_t)n;
}

/* 把 seen=1（收到过下发 => 物模型里确实有该属性）的执行器追加进 params */
static size_t append_seen_actuators(char *buf, size_t cap, size_t k, uint8_t *first) {
  size_t i;

  for (i = 0; i < ACTUATOR_COUNT; i++) {
    Actuator_t *a = &s_actuators[i];
#if !APP_REPORT_ACTUATOR_STATE
    if (!a->seen)
      continue;
#endif
    k = append_actuator(buf, cap, k, first, a);
  }
  return k;
}

/* 单独上报执行器状态（上电连上 OneNET 后调一次；周期上报里也会带上）*/
void Device_ReportActuatorState(void) {
  char params[192];
  size_t k = 0;
  uint8_t first = 1;
  int n;

  if (!OneNET_IsConnected())
    return;

  if (snprintf(params, sizeof params, "{") <= 0)
    return;
  k = 1;

  k = append_seen_actuators(params, sizeof params, k, &first);
  if (k == 1) /* 只有 "{"：没有任何可报的项 */
    return;

  n = snprintf(params + k, sizeof params - k, "}");
  if (n <= 0 || n >= (int)(sizeof params - k))
    return;
  OneNET_PublishProperty(params);
}

/* ============================================================
 *  下行：属性设置
 * ============================================================ */
static void on_property_set(const char *topic, const char *payload, size_t len) {
  char id[24];
  const cJSON *jid, *params;
  cJSON *root;
  size_t i;
  uint8_t changed = 0;

  (void)topic;

  root = cJSON_ParseWithLength(payload, len);
  if (root == NULL) {
    printf("[DEV] set 报文不是合法 JSON（%u 字节）：%s\r\n", (unsigned)len,
           payload);
    return;
  }

  jid = cJSON_GetObjectItem(root, "id");
  sanitize_id(cJSON_IsString(jid) ? jid->valuestring : NULL, id, sizeof id);

  params = cJSON_GetObjectItem(root, "params");
  if (!cJSON_IsObject(params)) {
    printf("[DEV] set 报文里没有 params 对象\r\n");
    cJSON_Delete(root);
    reply_property_set(id);
    return;
  }

  for (i = 0; i < ACTUATOR_COUNT; i++) {
    Actuator_t *a = &s_actuators[i];
    const cJSON *item = cJSON_GetObjectItem(params, a->id);
    uint8_t on;

    if (item == NULL)
      continue; /* 这次没下发这一项 */

    a->seen = 1;
    if (parse_switch(item, &on)) {
      actuator_apply(a, on);
      changed = 1;
    }
  }

  if (!changed)
    printf("[DEV] set 里没有本设备认得且能执行的标识符\r\n");

  cJSON_Delete(root);
  reply_property_set(id);
  /* 状态不在这里单独发：紧跟着 set_reply 再发一条 MQTTPUBRAW，ESP8266 会回
   * ERROR（实测 +MQTTPUB:OK 后跟 ERROR），平台收不到。状态并进主循环里
   * 5 秒一次的温湿度上报（Device_ReportTempHumi），一条发布带全部数据。*/
}

/* ============================================================
 *  下行：属性上报的回执（thing/property/post/reply）
 *  每 5 秒一条，成功(code=200)必须静默，否则日志被刷满、
 *  真正的失败（如 2402 格式错）反而被淹。
 * ============================================================ */
static void on_property_post_reply(const char *topic, const char *payload,
                                   size_t len) {
  const cJSON *code, *msg;
  cJSON *root;

  (void)topic;

  root = cJSON_ParseWithLength(payload, len);
  if (root == NULL) {
    printf("[DEV] 上报回执不是合法 JSON：%s\r\n", payload);
    return;
  }

  code = cJSON_GetObjectItem(root, "code");
  if (!cJSON_IsNumber(code) || code->valueint != 200) {
    msg = cJSON_GetObjectItem(root, "msg");
    printf("[DEV] 上报被平台拒绝：code=%d msg=%s\r\n",
           cJSON_IsNumber(code) ? code->valueint : -1,
           (msg && cJSON_IsString(msg)) ? msg->valuestring : "?");
  }

  cJSON_Delete(root);
}

/* ============================================================
 *  下行：物模型服务调用（thing/service/<identifier>）
 *  当前实现 reboot：先把应答发出去，再复位 MCU。
 *  下行 topic 实测为 …/thing/service/<identifier>/invoke，应答 …/invoke_reply。
 * ============================================================ */
#define SERVICE_REBOOT_DELAY_MS 300U

static void on_service_invoke(const char *topic, const char *payload, size_t len) {
  const char *svc = strstr(topic, "/thing/service/");
  const char *end;
  char name[32];
  char id[24];
  char resp_topic[192];
  char resp[192];
  cJSON *root;
  const cJSON *jid;
  uint8_t is_reboot;
  size_t n;

  /* topic 形如 …/thing/service/<identifier>/invoke，标识符取 /invoke 之前那段 */
  if (svc != NULL) {
    svc += strlen("/thing/service/");
    end = strstr(svc, "/invoke");
    n = (end != NULL) ? (size_t)(end - svc) : strlen(svc);
    if (n >= sizeof name)
      n = sizeof name - 1;
    memcpy(name, svc, n);
    name[n] = '\0';
  } else {
    name[0] = '\0';
  }
  is_reboot = (strcmp(name, "reboot") == 0);

  root = cJSON_ParseWithLength(payload, len);
  jid = root ? cJSON_GetObjectItem(root, "id") : NULL;
  sanitize_id(cJSON_IsString(jid) ? jid->valuestring : NULL, id, sizeof id);
  if (root)
    cJSON_Delete(root);

  printf("[DEV] 服务调用：%s（%s）\r\n", name, is_reboot ? "重启" : "不支持");

  /* data 字段必须带，缺了平台判 "response invalid"（实测） */
  snprintf(resp, sizeof resp,
           "{\"id\":\"%s\",\"code\":%d,\"msg\":\"%s\",\"data\":{}}", id,
           is_reboot ? 200 : 400, is_reboot ? "success" : "unsupported");

  /* 服务应答 topic = 收到的 topic + "_reply"
   * （…/thing/service/reboot/invoke -> …/reboot/invoke_reply，与属性 set -> set_reply 同规矩）*/
  snprintf(resp_topic, sizeof resp_topic, "%s_reply", topic);
  if (OneNET_PublishAbsolute(resp_topic, resp) != ONENET_OK)
    printf("[DEV] 服务应答发送失败\r\n");

  if (is_reboot) {
    printf("[DEV] rebooting...\r\n");
    HAL_Delay(SERVICE_REBOOT_DELAY_MS); /* 等应答与 AT 收发收尾 */
    NVIC_SystemReset();
  }
}

/* ============================================================
 *  下行：命令（cmd/request/<id>）—— 需在 OneNET 侧订阅后才有数据
 * ============================================================ */
static void on_cmd(const char *topic, const char *payload, size_t len) {
  const char *p = strstr(topic, "/cmd/request/");
  char id[24];
  char resp_topic[64];
  char resp[160];
  cJSON *root;

  sanitize_id(p ? p + strlen("/cmd/request/") : NULL, id, sizeof id);

  root = cJSON_ParseWithLength(payload, len);
  if (root != NULL) {
    const cJSON *cmd = cJSON_GetObjectItem(root, "cmd");
    if (cJSON_IsString(cmd)) {
      Actuator_t *lamp = &s_actuators[0]; /* 命令语义：第一路灯 */
      if (!strcmp(cmd->valuestring, "led_on")) {
        actuator_apply(lamp, 1);
      } else if (!strcmp(cmd->valuestring, "led_off")) {
        actuator_apply(lamp, 0);
      } else {
        printf("[DEV] 未知 cmd：%s\r\n", cmd->valuestring);
      }
    }
    cJSON_Delete(root);
  }

  snprintf(resp_topic, sizeof resp_topic, "/cmd/response/%s", id);
  snprintf(resp, sizeof resp, "{\"id\":\"%s\",\"code\":200,\"msg\":\"success\"}",
           id);
  if (OneNET_Publish(resp_topic, resp) != ONENET_OK)
    printf("[DEV] cmd 回执发送失败\r\n");
}

/* ============================================================
 *  注册 / 上报
 * ============================================================ */
static const OnenetHandler s_handlers[] = {
    {.topic_suffix = "/thing/property/set", .handler = on_property_set,
     .match_anywhere = 0},
    {.topic_suffix = "/thing/property/post/reply",
     .handler = on_property_post_reply, .match_anywhere = 0},
    /* /thing/service/<identifier> 尾部是变量，用子串匹配 */
    {.topic_suffix = "/thing/service/", .handler = on_service_invoke,
     .match_anywhere = 1},
    /* topic 尾部是变量 id，用子串匹配 */
    {.topic_suffix = "/cmd/request/", .handler = on_cmd, .match_anywhere = 1},
};

void Device_Init(void) {
  size_t i;

  /* 只注册下行处理表。引脚的时钟/模式/初始电平由 CubeMX 的 MX_GPIO_Init()
   * 配置，这里不做任何 GPIO 初始化。 */

  /* 用引脚真实电平初始化本地状态：反复电后平台上的值不会停在旧状态 */
  for (i = 0; i < ACTUATOR_COUNT; i++)
    s_actuators[i].state = actuator_read(&s_actuators[i]);

  printf("[DEV] 上电状态: led1=%u led2=%u led3=%u buzzer=%u fan=%u\r\n",
         (unsigned)s_actuators[0].state, (unsigned)s_actuators[1].state,
         (unsigned)s_actuators[2].state, (unsigned)s_actuators[3].state,
         (unsigned)s_actuators[4].state);

  OneNET_SetHandlers(s_handlers, sizeof s_handlers / sizeof s_handlers[0]);
}

void Device_ReportTempHumi(void) {
  DHT11_Data_t d = DHT11_GetData();
  char params[256];
  size_t k;
  uint8_t first = 0; /* 温度已经占了第一项，后面都要前导逗号 */
  int n;

  n = snprintf(params, sizeof params,
               "{\"temperature\":{\"value\":%d.%d},"
               "\"humidity\":{\"value\":%d.%d}",
               d.temp_int, d.temp_dec, d.humi_int, d.humi_dec);
  if (n <= 0 || n >= (int)sizeof params) {
    printf("[DEV] 上报 params 组装失败\r\n");
    return;
  }
  k = (size_t)n;

  /* 执行器状态搭同一条发布上报：面板轮询就能看到灯/蜂鸣器/风扇的真实状态，
   * 也避免在下发回执后紧跟第二条 MQTTPUBRAW（ESP8266 会回 ERROR）。*/
  k = append_seen_actuators(params, sizeof params, k, &first);
  if (k + 2 >= sizeof params) {
    printf("[DEV] 上报过长，跳过执行器状态\r\n");
    k = (size_t)n;
  }
  params[k++] = '}';
  params[k] = '\0';

  if (OneNET_PublishProperty(params) != ONENET_OK)
    printf("[DEV] 温湿度上报失败\r\n");
}
