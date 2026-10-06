#ifndef __BSP_ESP8266_H
#define __BSP_ESP8266_H

#include "main.h"
#include <stddef.h>
#include <stdint.h>

/* ============ 可配置参数 ============ */
#define ESP8266_RX_BUF_SIZE 1024 // 环形缓冲大小（中断里写）
#define ESP8266_ACC_SIZE 2048    // 累积文本缓冲大小（主循环里读）
#define ESP8266_LINE_MAX 256     // BSP_ESP8266_GetLine 的单行上限

/* ============ 返回值 ============ */
#define ESP_OK 0
#define ESP_ERR_TIMEOUT 1
#define ESP_ERR_FAIL 2

/* ============ 初始化 / 发送 ============ */

/* 初始化：传入 CubeMX 生成的 UART 句柄（如 &huart4） */
void BSP_ESP8266_Init(UART_HandleTypeDef *huart);

/* 发送 AT 指令（自动追加 \r\n） */
void BSP_ESP8266_SendAT(const char *cmd);

/* 发送原始数据（AT 交互的响应体、MQTT payload 等） */
void BSP_ESP8266_SendData(const uint8_t *data, uint16_t len);

/* 中断回调里调用（收到一个字节） */
void BSP_ESP8266_RxCallback(uint8_t data);

/* 清空所有缓冲（发 AT 指令前调用） */
void BSP_ESP8266_ClearBuffer(void);

/* ============ 读缓冲（内部先 pump 环形缓冲，再看累积缓冲） ============ */

/* 当前缓冲里是否包含某字符串，返回 1/0 */
uint8_t BSP_ESP8266_Contains(const char *expected);

/* 在缓冲里查找字符串，返回指针（找不到返回 NULL） */
char *BSP_ESP8266_Find(const char *pattern);

/* 累积缓冲首地址（只读；按长度解析的调用方用它算偏移） */
char *BSP_ESP8266_GetBuffer(void);

/* 累积缓冲里的有效字节数 */
uint16_t BSP_ESP8266_GetLength(void);

/* 从累积缓冲头部丢掉 n 字节（消费已处理的数据；n 超长时清空） */
void BSP_ESP8266_Consume(uint16_t n);

/* 取一行（\n 结尾，已去掉 \r）；没有完整一行返回 NULL。
 * 返回的是 BSP 内部静态缓冲，下一次调用即失效。 */
char *BSP_ESP8266_GetLine(void);

/* ============ 同步等待 ============ */

/* 轮询等待：命中 expect → ESP_OK；命中 err1/err2（传 NULL 表示不检查）→
 * ESP_ERR_FAIL；超时 → ESP_ERR_TIMEOUT */
uint8_t BSP_ESP8266_WaitFor(const char *expect, const char *err1, const char *err2,
                           uint32_t timeout);

/* 清缓冲 + 发一条 AT + 等 expect（错误关键字固定识别 ERROR / FAIL） */
uint8_t BSP_ESP8266_SendAT_Wait(const char *cmd, const char *expect, uint32_t timeout);

/* 发 AT → 等 prompt（如 ">"）→ 发原始数据 → 等 expect。
 * 供 MQTTPUBRAW / HTTPPOST 这类"先握手、再送数据"的指令用。 */
uint8_t BSP_ESP8266_SendAT_WaitThenData(const char *cmd, const char *prompt,
                                       const uint8_t *data, uint16_t len,
                                       const char *expect, uint32_t prompt_timeout,
                                       uint32_t ack_timeout);

/* ============ 诊断计数（排查"发 AT 没响应"用） ============ */

uint32_t BSP_ESP8266_RxCount(void);   /* 收到的总字节数（中断里累加） */
uint32_t BSP_ESP8266_DropCount(void); /* 装不下而丢掉的字节数（环形缓冲满 + 累积缓冲满） */
uint32_t BSP_ESP8266_ErrCount(void);  /* UART 出错次数（ORE/FE/NE/PE） */
uint32_t BSP_ESP8266_LastError(void); /* 最后一次 HAL 错误码 */

#endif /* __BSP_ESP8266_H */
