/**
 * @file ws_server.h
 *
 * WebSocket 服务 — 实时推送设备数据到客户端
 */
#ifndef E2_WS_SERVER_H
#define E2_WS_SERVER_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** 初始化 WebSocket 服务（挂载到现有 HTTP 服务器）. */
int  ws_server_init(void);

/** 关闭 WebSocket 服务. */
void ws_server_shutdown(void);

/** 广播设备数据点到所有连接的 WebSocket 客户端. */
void ws_broadcast_datapoint(const char *device_id, const char *metric,
                            double value, uint64_t ts);

/** 广播告警事件. */
void ws_broadcast_alarm(const char *device_id, const char *metric,
                        double value, double threshold, int severity);

#ifdef __cplusplus
}
#endif

#endif /* E2_WS_SERVER_H */
