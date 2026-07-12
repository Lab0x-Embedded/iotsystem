/**
 * @file sse_handler.h
 *
 * SSE (Server-Sent Events) — 实时推送设备数据到客户端
 * 挂载到现有 HTTP 服务器，无需额外端口/线程
 */
#ifndef E2_SSE_HANDLER_H
#define E2_SSE_HANDLER_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** 注册 SSE 路由到 HTTP 服务器. */
int  sse_handler_init(void);

/** 广播设备数据点到所有 SSE 客户端. */
void sse_broadcast_datapoint(const char *device_id, const char *metric,
                             double value, uint64_t ts);

/** 广播告警事件. */
void sse_broadcast_alarm(const char *device_id, const char *metric,
                         double value, double threshold, int severity);

#ifdef __cplusplus
}
#endif

#endif /* E2_SSE_HANDLER_H */
