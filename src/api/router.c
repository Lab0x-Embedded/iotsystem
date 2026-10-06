/**
 * @file router.c
 *
 * HTTP 路由注册 — 静态表实现 + 分发 + REST 路由表
 */
#include "api/router.h"
#include "api/auth_middleware.h"
#include "api/handlers.h"
#include "api/http_server.h"
#include "common/log.h"

#include <string.h>
#include <stdlib.h>

#include <event2/http.h>

#define MAX_ROUTES 32

typedef struct {
    char method[8];
    char path[128];
    http_handler_t handler;
    int  require_auth;
} route_t;

static route_t g_routes[MAX_ROUTES];
static int     g_count = 0;

int router_init(void) {
    memset(g_routes, 0, sizeof(g_routes));
    g_count = 0;
    LOG_INFO("router initialized");
    return 0;
}

void router_register(const char *method, const char *path,
                     http_handler_t handler, int require_auth) {
    if (!method || !path || !handler || g_count >= MAX_ROUTES) return;
    route_t *r = &g_routes[g_count++];
    strncpy(r->method, method, sizeof(r->method) - 1);
    strncpy(r->path, path, sizeof(r->path) - 1);
    r->handler = handler;
    r->require_auth = require_auth;
    LOG_INFO("route registered: %s %s (auth=%d)", method, path, require_auth);
}

http_handler_t router_resolve(const char *method, const char *path,
                              int *require_auth) {
    if (!method || !path) return NULL;
    for (int i = 0; i < g_count; i++) {
        if (strcmp(g_routes[i].method, method) == 0 &&
            strcmp(g_routes[i].path, path) == 0) {
            if (require_auth) *require_auth = g_routes[i].require_auth;
            return g_routes[i].handler;
        }
    }
    return NULL;
}

/* ----------------------------------------------------------------- */
/* 内部辅助: 从 URI 中剥离 query string, 只保留 path                     */
/* ----------------------------------------------------------------- */
static char *extract_path(const char *uri) {
    if (!uri) return NULL;
    const char *q = strchr(uri, '?');
    size_t n = q ? (size_t)(q - uri) : strlen(uri);
    char *p = malloc(n + 1);
    if (!p) return NULL;
    memcpy(p, uri, n);
    p[n] = '\0';
    return p;
}

/* ----------------------------------------------------------------- */
/* 分发: resolve → auth → handler                                      */
/* ----------------------------------------------------------------- */
void router_dispatch(struct evhttp_request *req) {
    if (!req) return;

    const char *method = http_method_str(evhttp_request_get_command(req));
    const char *uri    = evhttp_request_get_uri(req);
    char *path = extract_path(uri);
    if (!path) {
        LOG_WARN("dispatch: extract_path failed for uri=%s", uri ? uri : "(null)");
        evhttp_send_error(req, 500, "Internal Error");
        return;
    }

    int require_auth = 0;
    http_handler_t h = router_resolve(method, path, &require_auth);

    LOG_INFO("HTTP %s %s -> %s (auth=%d)", method, path,
             h ? "matched" : "404", require_auth);

    if (!h) {
        free(path);
        http_reply_json(req, 404, "Not Found",
                        "{\"error\":\"no such route\"}");
        return;
    }

    if (require_auth && auth_middleware_check(req) != 0) {
        free(path);
        struct evkeyvalq *hdrs = evhttp_request_get_output_headers(req);
        evhttp_add_header(hdrs, "WWW-Authenticate", "Bearer");
        http_reply_json(req, 401, "Unauthorized",
                        "{\"error\":\"missing or invalid token\"}");
        return;
    }

    h(req, NULL);
    free(path);
}

/* ----------------------------------------------------------------- */
/* REST 路由表                                                          */
/* ----------------------------------------------------------------- */
typedef struct {
    const char *method;
    const char *path;
    http_handler_t handler;
    int require_auth;
} rest_route_t;

void router_register_rest_routes(void) {
    static const rest_route_t routes[] = {
        { "POST", "/api/device",  handler_device,  1 },
        { "POST", "/api/shadow",  handler_shadow,  1 },
        { "POST", "/api/command", handler_command, 1 },
        { "POST", "/api/alarm",   handler_alarm,   1 },
        { "POST", "/api/user",    handler_user,    0 },
        { "POST", "/api/onenet",  handler_onenet,  0 },
        { "POST", "/api/group",   handler_group,   1 },
        { "POST", "/api/product", handler_product, 1 },
    };
    for (size_t i = 0; i < sizeof(routes) / sizeof(routes[0]); i++) {
        router_register(routes[i].method, routes[i].path,
                        routes[i].handler, routes[i].require_auth);
    }
}
