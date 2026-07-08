/**
 * @file router.h
 *
 * HTTP 路由注册 — 路径 → handler 映射 + 分发
 *
 * 设计:
 *   - 方法 + 路径 → 处理函数 (router_register / router_resolve)
 *   - 支持中间件链: auth_middleware → handler (router_dispatch)
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

/**
 * 分发一个 HTTP 请求.
 *
 * 内部流程: router_resolve → 可选 auth_middleware_check → handler(ctx=NULL).
 * 未匹配 → 404. auth 失败 → 401.
 *
 * @param req  libevent HTTP 请求对象 (必须非 NULL)
 */
void router_dispatch(struct evhttp_request *req);

/**
 * 注册全部 REST API 路由.
 * 在 http_server_start 之前调用一次即可.
 */
void router_register_rest_routes(void);

#ifdef __cplusplus
}
#endif

#endif /* E2_ROUTER_H */
