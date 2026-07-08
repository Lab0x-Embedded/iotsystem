/**
 * @file auth_middleware.c
 *
 * JWT 鉴权中间件 — 桩实现 (始终放行)
 */
#include "api/auth_middleware.h"
#include "common/log.h"

#include <event2/keyvalq_struct.h>

int auth_middleware_check(struct evhttp_request *req) {
    if (!req) return -1;
    struct evkeyvalq *hdrs = evhttp_request_get_input_headers(req);
    const char *auth = evhttp_find_header(hdrs, "Authorization");
    if (!auth) {
        LOG_WARN("auth: missing Authorization header");
        return -1;
    }
    if (strncmp(auth, "Bearer ", 7) != 0) {
        LOG_WARN("auth: malformed Authorization header");
        return -1;
    }
    /* 真实实现: 解析 JWT, 校验签名与 exp */
    return 0;
}

int auth_middleware_generate_token(const char *user_id, char *out, size_t cap) {
    if (!user_id || !out || cap == 0) return -1;
    snprintf(out, cap, "stub.jwt.%s", user_id);
    return 0;
}
