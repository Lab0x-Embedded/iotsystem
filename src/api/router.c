/**
 * @file router.c
 *
 * HTTP 路由注册 — 静态表实现
 */
#include "api/router.h"
#include "common/log.h"

#include <string.h>

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
