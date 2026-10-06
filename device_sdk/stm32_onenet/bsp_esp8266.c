#include "bsp_esp8266.h"
#include <string.h>

/* ================= 内部全局变量 ================= */

static UART_HandleTypeDef *esp_huart = NULL;

/* 环形缓冲：中断里写，主循环里读 */
static uint8_t esp_rx_buf[ESP8266_RX_BUF_SIZE];
static volatile uint16_t esp_rx_head = 0;
static volatile uint16_t esp_rx_tail = 0;

/* 累积文本缓冲：主循环里做字符串查找 / 按长度解析 */
static char esp_acc[ESP8266_ACC_SIZE];
static uint16_t esp_acc_len = 0;

/* 中断里单字节接收（模块内部状态，不再对外暴露） */
static uint8_t esp_rx_byte;

/* 诊断计数 */
static volatile uint32_t esp_rx_total = 0;  /* 收到的总字节数 */
static volatile uint32_t esp_rx_drop = 0;   /* 装不下丢掉的字节数 */
static volatile uint32_t esp_err_total = 0; /* UART 错误次数 */
static volatile uint32_t esp_last_err = 0;  /* 最后一次 HAL 错误码 */

/* ================= 内部函数 ================= */

/* 把环形缓冲里的新字节搬到累积缓冲；累积缓冲满时丢字节并计数 */
static void esp_pump(void) {
  uint16_t head = esp_rx_head;
  uint16_t tail = esp_rx_tail;

  if (head == tail)
    return;

  while (tail != head) {
    if (esp_acc_len < ESP8266_ACC_SIZE - 1) {
      esp_acc[esp_acc_len++] = (char)esp_rx_buf[tail];
    } else {
      /* 以前这里静默丢弃：排障时数据凭空消失。现在计入 drop，
       * 表现是"收到的字节数 > 缓冲里的字节数 + 丢掉数"能对上账 */
      esp_rx_drop++;
    }
    tail = (tail + 1) % ESP8266_RX_BUF_SIZE;
  }
  esp_acc[esp_acc_len] = '\0';
  esp_rx_tail = head;
}

/* ================= 对外接口 ================= */

void BSP_ESP8266_Init(UART_HandleTypeDef *huart) {
  esp_huart = huart;

  esp_rx_head = 0;
  esp_rx_tail = 0;
  esp_acc_len = 0;
  esp_acc[0] = '\0';

  /* 启动第一次单字节中断接收，后续在回调里自动续接 */
  HAL_UART_Receive_IT(esp_huart, &esp_rx_byte, 1);
}

/* HAL 单字节接收完成回调：存入环形缓冲，并立刻续上下一次接收 */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
  if (huart == esp_huart) {
    BSP_ESP8266_RxCallback(esp_rx_byte);
  }
}

void BSP_ESP8266_SendAT(const char *cmd) {
  /* 没 Init 就调这里是调用方的 bug：以前会拿 NULL 句柄进 HAL 直接 hardfault */
  if (esp_huart == NULL || cmd == NULL)
    return;

  HAL_UART_Transmit(esp_huart, (uint8_t *)cmd, (uint16_t)strlen(cmd),
                    HAL_MAX_DELAY);
  HAL_UART_Transmit(esp_huart, (uint8_t *)"\r\n", 2, HAL_MAX_DELAY);
}

void BSP_ESP8266_SendData(const uint8_t *data, uint16_t len) {
  if (esp_huart == NULL || data == NULL || len == 0)
    return;
  HAL_UART_Transmit(esp_huart, (uint8_t *)data, len, HAL_MAX_DELAY);
}

void BSP_ESP8266_RxCallback(uint8_t data) {
  uint16_t next_head = (esp_rx_head + 1) % ESP8266_RX_BUF_SIZE;

  esp_rx_total++;

  /* 缓冲满就丢掉这个字节，避免覆盖未读数据 */
  if (next_head != esp_rx_tail) {
    esp_rx_buf[esp_rx_head] = data;
    esp_rx_head = next_head;
  } else {
    esp_rx_drop++;
  }

  /* 重新开启下一次中断接收 */
  if (esp_huart) {
    HAL_UART_Receive_IT(esp_huart, &esp_rx_byte, 1);
  }
}

void BSP_ESP8266_ClearBuffer(void) {
  esp_rx_tail = esp_rx_head;
  esp_acc_len = 0;
  esp_acc[0] = '\0';
}

void BSP_ESP8266_Consume(uint16_t n) {
  if (n == 0)
    return;

  esp_pump();

  if (n >= esp_acc_len) {
    esp_acc_len = 0;
  } else {
    memmove(esp_acc, esp_acc + n, (size_t)(esp_acc_len - n));
    esp_acc_len = (uint16_t)(esp_acc_len - n);
  }
  esp_acc[esp_acc_len] = '\0';
}

/*------------------------------------------------------------------
 * 关键：HAL 的单字节中断接收遇到 溢出(ORE)/帧错(FE)/噪声(NE) 会把接收链
 * 掐断（HAL 调 ErrorCallback，之后没人重新 arm）——表现就是"发了 AT 没反应、
 * 缓冲区永远是空的、ret 全是超时"。ESP8266 上电/复位时会吐 74880 波特率的
 * 乱码，正好制造帧错，所以这个回调必须有。
 *----------------------------------------------------------------*/
void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart) {
  if (huart == esp_huart) {
    esp_err_total++;
    esp_last_err = (uint32_t)HAL_UART_GetError(huart);

    __HAL_UART_CLEAR_OREFLAG(esp_huart);             /* ORE 不清会立刻复发 */
    esp_huart->RxState = HAL_UART_STATE_READY;       /* 强制回到可接收状态 */
    HAL_UART_Receive_IT(esp_huart, &esp_rx_byte, 1); /* 把接收链接回去 */
  }
}

uint32_t BSP_ESP8266_RxCount(void) { return esp_rx_total; }
uint32_t BSP_ESP8266_DropCount(void) { return esp_rx_drop; }
uint32_t BSP_ESP8266_ErrCount(void) { return esp_err_total; }
uint32_t BSP_ESP8266_LastError(void) { return esp_last_err; }

char *BSP_ESP8266_GetBuffer(void) {
  esp_pump();
  return esp_acc;
}

uint16_t BSP_ESP8266_GetLength(void) {
  esp_pump();
  return esp_acc_len;
}

uint8_t BSP_ESP8266_Contains(const char *expected) {
  if (expected == NULL)
    return 0;
  esp_pump();
  return (strstr(esp_acc, expected) != NULL) ? 1 : 0;
}

char *BSP_ESP8266_Find(const char *pattern) {
  if (pattern == NULL)
    return NULL;
  esp_pump();
  return strstr(esp_acc, pattern);
}

/* ============================================================
 *          阻塞式等待期望响应（唯一的一份等待实现）
 *  返回：
 *    ESP_OK          命中 expect
 *    ESP_ERR_FAIL    命中 err1 / err2
 *    ESP_ERR_TIMEOUT 超时
 * ============================================================ */
uint8_t BSP_ESP8266_WaitFor(const char *expect, const char *err1, const char *err2,
                           uint32_t timeout) {
  uint32_t start = HAL_GetTick();

  while (HAL_GetTick() - start < timeout) {
    if (BSP_ESP8266_Contains(expect))
      return ESP_OK;
    if (err1 && BSP_ESP8266_Contains(err1))
      return ESP_ERR_FAIL;
    if (err2 && BSP_ESP8266_Contains(err2))
      return ESP_ERR_FAIL;
    HAL_Delay(10);
  }
  return ESP_ERR_TIMEOUT;
}

/* 清缓冲 + 发 AT + 等响应 */
uint8_t BSP_ESP8266_SendAT_Wait(const char *cmd, const char *expect,
                                uint32_t timeout) {
  BSP_ESP8266_ClearBuffer();
  BSP_ESP8266_SendAT(cmd);
  return BSP_ESP8266_WaitFor(expect, "ERROR", "FAIL", timeout);
}

/* 发 AT → 等 prompt → 发数据 → 等 ack（MQTTPUBRAW 就是这套握手） */
uint8_t BSP_ESP8266_SendAT_WaitThenData(const char *cmd, const char *prompt,
                                       const uint8_t *data, uint16_t len,
                                       const char *expect, uint32_t prompt_timeout,
                                       uint32_t ack_timeout) {
  uint8_t ret;

  BSP_ESP8266_ClearBuffer();
  BSP_ESP8266_SendAT(cmd);

  /* 把真实错误码带回去：超时(1) 和模块报错(2) 的下一步排查完全不同 */
  ret = BSP_ESP8266_WaitFor(prompt, "ERROR", "FAIL", prompt_timeout);
  if (ret != ESP_OK)
    return ret;

  BSP_ESP8266_SendData(data, len);

  return BSP_ESP8266_WaitFor(expect, "ERROR", "FAIL", ack_timeout);
}

char *BSP_ESP8266_GetLine(void) {
  static char line[ESP8266_LINE_MAX];

  esp_pump(); /* 把环形缓冲的新字节搬进 esp_acc */

  /* 找一行结束（\n） */
  char *nl = memchr(esp_acc, '\n', esp_acc_len);
  if (nl == NULL)
    return NULL; /* 还没有完整一行 */

  size_t line_len = (size_t)(nl - esp_acc);

  /* 复制这行，去掉结尾 \r */
  size_t copy_len = line_len;
  if (copy_len > 0 && esp_acc[copy_len - 1] == '\r')
    copy_len--;
  if (copy_len >= sizeof(line))
    copy_len = sizeof(line) - 1;
  memcpy(line, esp_acc, copy_len);
  line[copy_len] = '\0';

  /* 把这一行（含 \n）从累积缓冲里消费掉 */
  BSP_ESP8266_Consume((uint16_t)(line_len + 1));

  return line;
}
