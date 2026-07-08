/**
 * @file router.h
 *
 * HTTP 路由注册 — 路径 → handler 映射
 *
 * 设计:
 *   - 方法 + 路径 → 处理函数
 *   - 支持中间件链: auth_middleware → handler
 */
#ifndef E2_ROUTER_H
#define E2_ROUTER_H

#include "http_server.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*http_handler_t)(struct evhttp_request *, void *ctx);

int  router_init(void);

/** 注册路由. */
void router_register(const char *method, const char *path,
                     http_handler_t handler, int require_auth);

/** 根据 method+path 查找 handler; 未找到返回 NULL. */
http_handler_t router_resolve(const char *method, const char *path,
                              int *require_auth);

#ifdef __cplusplus
}
#endif

#endif /* E2_ROUTER_H */
