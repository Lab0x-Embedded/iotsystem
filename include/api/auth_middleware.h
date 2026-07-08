/**
 * @file auth_middleware.h
 *
 * JWT 鉴权中间件 — 校验 Authorization: Bearer <token>
 *
 * 设计:
 *   - 解析 Bearer token
 *   - 校验签名 + 过期时间
 *   - 失败返回 401
 */
#ifndef E2_AUTH_MIDDLEWARE_H
#define E2_AUTH_MIDDLEWARE_H

#include <event2/http.h>

#ifdef __cplusplus
extern "C" {
#endif

/** 校验请求中的 JWT; 返回 0 通过, -1 拒绝. */
int auth_middleware_check(struct evhttp_request *req);

/** 生成 JWT (测试用). */
int auth_middleware_generate_token(const char *user_id, char *out, size_t cap);

#ifdef __cplusplus
}
#endif

#endif /* E2_AUTH_MIDDLEWARE_H */
