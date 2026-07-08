/**
 * @file http_server.h
 *
 * P5 指令下行 — HTTP REST API
 *
 * 启动独立线程监听 :8080, 提供 POST /api/command
 */
#ifndef E2_HTTP_SERVER_H
#define E2_HTTP_SERVER_H

#ifdef __cplusplus
extern "C" {
#endif

/** 启动 HTTP API 监听线程 (port=8080). 返回 0 成功, -1 失败. */
int http_server_start(int port);

/** 停止 HTTP API 线程. */
void http_server_stop(void);

#ifdef __cplusplus
}
#endif

#endif /* E2_HTTP_SERVER_H */
