/**
 * @file test_router.c — 路由注册/解析/分发逻辑单元测试
 *
 * 不依赖 libevent: 直接调用 router_register / router_resolve,
 * 并用一个 fake handler 验证 dispatch 路径 (auth 分支通过桩覆盖).
 *
 * 注意: router_dispatch 需要 evhttp_request 对象, 这里只验证
 *       register/resolve 的核心逻辑; dispatch 的 auth 分支通过
 *       集成测试 (test_phase4 风格) 覆盖.
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "api/router.h"

/* 测试用 fake handler */
static int g_called = 0;
static void fake_handler(struct evhttp_request *req, void *ctx) {
    (void)req; (void)ctx;
    g_called++;
}

/* 另一个 handler, 用于区分路由 */
static int g_other_called = 0;
static void other_handler(struct evhttp_request *req, void *ctx) {
    (void)req; (void)ctx;
    g_other_called++;
}

static int check(int cond, const char *label) {
    printf("  %s -> %s\n", label, cond ? "PASS" : "FAIL");
    return cond ? 0 : 1;
}

int main(void) {
    int fails = 0;

    printf("=== router unit test ===\n");

    /* 初始状态: 空表 */
    router_init();
    int auth = 0;
    fails += check(router_resolve("POST", "/api/device", &auth) == NULL,
                   "resolve empty table -> NULL");

    /* 注册一条路由 */
    router_register("POST", "/api/device", fake_handler, 1);
    auth = -1;
    fails += check(router_resolve("POST", "/api/device", &auth) == fake_handler,
                   "resolve POST /api/device -> fake_handler");
    fails += check(auth == 1, "resolve auth flag = 1");

    /* method 不匹配 */
    auth = -1;
    fails += check(router_resolve("GET", "/api/device", &auth) == NULL,
                   "resolve GET /api/device -> NULL");

    /* path 不匹配 */
    fails += check(router_resolve("POST", "/api/unknown", &auth) == NULL,
                   "resolve POST /api/unknown -> NULL");

    /* 注册多条 */
    router_register("POST", "/api/shadow", other_handler, 1);
    router_register("POST", "/api/command", fake_handler, 1);
    router_register("POST", "/api/user", fake_handler, 0);
    router_register("POST", "/api/onenet", fake_handler, 0);

    auth = -1;
    fails += check(router_resolve("POST", "/api/shadow", &auth) == other_handler,
                   "resolve POST /api/shadow -> other_handler");
    fails += check(auth == 1, "shadow auth=1");

    auth = -1;
    fails += check(router_resolve("POST", "/api/user", &auth) == fake_handler,
                   "resolve POST /api/user -> fake_handler");
    fails += check(auth == 0, "user auth=0");

    /* 不传 require_auth 指针 (允许 NULL) */
    fails += check(router_resolve("POST", "/api/command", NULL) == fake_handler,
                   "resolve with NULL require_auth -> fake_handler");

    /* 边界: 空 method / 空 path */
    fails += check(router_resolve(NULL, "/api/device", NULL) == NULL,
                   "resolve NULL method -> NULL");
    fails += check(router_resolve("POST", NULL, NULL) == NULL,
                   "resolve NULL path -> NULL");

    /* 边界: 注册非法参数 (不应 crash) */
    router_register(NULL, "/x", fake_handler, 0);
    router_register("POST", NULL, fake_handler, 0);
    router_register("POST", "/y", NULL, 0);
    fails += check(router_resolve("POST", "/x", NULL) == NULL,
                   "register NULL method -> not stored");
    fails += check(router_resolve("POST", "/y", NULL) == NULL,
                   "register NULL handler -> not stored");

    /* 重复注册: 两条同 method+path, 应命中第一条 */
    router_init();
    router_register("POST", "/dup", fake_handler, 0);
    router_register("POST", "/dup", other_handler, 0);
    fails += check(router_resolve("POST", "/dup", NULL) == fake_handler,
                   "duplicate register -> first wins");

    /* router_register_rest_routes 不应 crash 且注册 6 条 */
    router_init();
    router_register_rest_routes();
    fails += check(router_resolve("POST", "/api/device", NULL) != NULL,
                   "rest_routes: /api/device registered");
    fails += check(router_resolve("POST", "/api/shadow", NULL) != NULL,
                   "rest_routes: /api/shadow registered");
    fails += check(router_resolve("POST", "/api/command", NULL) != NULL,
                   "rest_routes: /api/command registered");
    fails += check(router_resolve("POST", "/api/alarm", NULL) != NULL,
                   "rest_routes: /api/alarm registered");
    fails += check(router_resolve("POST", "/api/user", NULL) != NULL,
                   "rest_routes: /api/user registered");
    fails += check(router_resolve("POST", "/api/onenet", NULL) != NULL,
                   "rest_routes: /api/onenet registered");

    printf("\n%d failures\n", fails);
    return fails;
}
