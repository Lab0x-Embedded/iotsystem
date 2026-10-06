#include "app_wifi.h"
#include "bsp_esp8266.h"

#include <stdio.h>
#include <string.h>

/* ============================================================
 *  从累积缓冲里抓最后一个 IPv4 地址，兼容新旧固件：
 *    - 新版：+CIFSR:STAIP,"192.168.1.100"
 *    - 旧版：192.168.1.100
 *    - AP+STA：APIP 和 STAIP 两个，取最后一个（一般是 STAIP）
 * ============================================================ */
static uint8_t wifi_parse_ip(char *dst, uint16_t max_len) {
  char *p = BSP_ESP8266_GetBuffer(); /* 内部会 pump 一次环形缓冲 */
  char *found = NULL;
  uint16_t found_len = 0, k;

  if (dst == NULL || max_len == 0)
    return 0;
  dst[0] = '\0';

  if (p == NULL)
    return 0;

  while (*p != '\0') {
    char *q = p;
    uint8_t seg, digits, ok = 1;

    if (!(*p >= '0' && *p <= '9')) {
      p++;
      continue;
    }

    for (seg = 0; seg < 4 && ok; seg++) {
      digits = 0;
      while (*q >= '0' && *q <= '9' && digits < 3) {
        q++;
        digits++;
      }
      if (digits == 0) {
        ok = 0;
        break;
      }
      if (seg < 3) {
        if (*q != '.') {
          ok = 0;
          break;
        }
        q++;
      }
    }
    if (ok && !(*q >= '0' && *q <= '9')) {
      found = p;
      found_len = (uint16_t)(q - p);
      p = q;
    } else {
      p++;
    }
  }

  if (found == NULL)
    return 0;

  k = 0;
  while (k < found_len && k + 1 < max_len) {
    dst[k] = found[k];
    k++;
  }
  dst[k] = '\0';
  return 1;
}

/* ================= 对外接口 ================= */

WIFI_Status_t WIFI_Init(void) {
  BSP_ESP8266_ClearBuffer();
  BSP_ESP8266_SendAT("AT");
  /* 模块刚上电时还在启动，给 2 秒；这段时间也能收到它的启动信息 */
  if (BSP_ESP8266_WaitFor("OK", "ERROR", NULL, 2000) != ESP_OK)
    return WIFI_ERR_NO_MODULE;

  /* 关回显：不关的话后面每条指令都会被回显一遍，解析变复杂 */
  if (BSP_ESP8266_SendAT_Wait("ATE0", "OK", 1000) != ESP_OK)
    printf("[WIFI] ATE0 失败（回显可能还开着）\r\n");

  /* Station 模式 */
  if (BSP_ESP8266_SendAT_Wait("AT+CWMODE=1", "OK", 2000) != ESP_OK)
    return WIFI_ERR_AT_FAIL;

  return WIFI_OK;
}

WIFI_Status_t WIFI_Connect(const char *ssid, const char *pwd) {
  char cmd[128];

  if (ssid == NULL || pwd == NULL)
    return WIFI_ERR_AT_FAIL;

  snprintf(cmd, sizeof cmd, "AT+CWJAP=\"%s\",\"%s\"", ssid, pwd);

  BSP_ESP8266_ClearBuffer();
  BSP_ESP8266_SendAT(cmd);

  /* 连 WiFi 慢，给 15 秒；成功的关键字是 WIFI GOT IP */
  if (BSP_ESP8266_WaitFor("WIFI GOT IP", "FAIL", "ERROR", 15000) != ESP_OK)
    return WIFI_ERR_CONNECT_FAIL;

  /* 拿到 IP 后再让模块稳定一下，紧跟着发 AT+CIFSR 容易拿到空响应 */
  HAL_Delay(2000);
  return WIFI_OK;
}

WIFI_Status_t WIFI_GetIP(char *ip_buf, uint16_t len) {
  if (ip_buf == NULL || len == 0)
    return WIFI_ERR_NO_IP;

  ip_buf[0] = '\0';

  BSP_ESP8266_ClearBuffer();
  BSP_ESP8266_SendAT("AT+CIFSR");

  if (BSP_ESP8266_WaitFor("OK", "ERROR", NULL, 2000) != ESP_OK)
    return WIFI_ERR_TIMEOUT;

  return wifi_parse_ip(ip_buf, len) ? WIFI_OK : WIFI_ERR_NO_IP;
}
