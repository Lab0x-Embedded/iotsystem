/**
 * @file main.c — 主入口 (P1 + P4/P5/P6 集成)
 */
#include "common/log.h"
#include "common/config.h"
#include "server/event_loop.h"
#include "server/connection.h"
#include "server/thread_pool.h"
#include "server/mqtt_broker.h"
#include "api/http_server.h"
#include "api/router.h"
#include "business/device_manager.h"
#include "business/shadow_manager.h"
#include "business/alarm_service.h"
#include "data/db_pool.h"
#include "data/data_writer.h"
#include "server/sse_handler.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <getopt.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <errno.h>

static event_loop_t *g_loop;
thread_pool_t *g_thread_pool = NULL;
static void on_signal(int sig) { (void)sig; if (g_loop) event_loop_stop(g_loop); }

static int make_listener(int port, int backlog) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) { LOG_ERROR("socket: %s", strerror(errno)); return -1; }

    int yes = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port   = htons((uint16_t)port);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
        LOG_ERROR("bind :%d: %s", port, strerror(errno));
        close(fd);
        return -1;
    }
    if (listen(fd, backlog) != 0) {
        LOG_ERROR("listen: %s", strerror(errno));
        close(fd);
        return -1;
    }
    if (fcntl(fd, F_SETFL, O_NONBLOCK) != 0) {
        LOG_ERROR("fcntl O_NONBLOCK: %s", strerror(errno));
        close(fd);
        return -1;
    }
    return fd;
}

static void usage(const char *argv0) {
    fprintf(stderr,
        "Usage: %s [options]\n"
        "\n"
        "Options:\n"
        "  -p, --port P          MQTT port (default: 65080)\n"
        "  -H, --http N          HTTP API port (default: 8080)\n"
        "  -w, --workers N       Worker threads (default: 4)\n"
        "  -b, --backlog B       Listen backlog (default: 1024)\n"
        "  -c, --config FILE     Load config from JSON file\n"
        "  -D, --db-host HOST    MySQL host (default: 127.0.0.1)\n"
        "  -P, --db-port PORT    MySQL port (default: 3306)\n"
        "  -U, --db-user USER    MySQL user (default: root)\n"
        "  -W, --db-password PWD MySQL password\n"
        "  -N, --db-name DB      MySQL database (default: e2_iot)\n"
        "  -h, --help            Show this help\n"
        "\n"
        "Examples:\n"
        "  %s --config deploy/config.json\n"
        "  %s --db-host 192.168.1.100 --db-password secret\n",
        argv0, argv0, argv0);
}

int main(int argc, char **argv) {
    // 默认配置
    app_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.mqtt_port = 65080;
    cfg.http_port = 8080;
    cfg.workers = 4;
    cfg.backlog = 1024;
    cfg.db.host = "127.0.0.1";
    cfg.db.port = 3306;
    cfg.db.user = "root";
    cfg.db.password = "your_password";
    cfg.db.database = "e2_iot";
    cfg.db.pool_size = 4;

    // 先检查是否有 --config 参数
    for (int i = 1; i < argc; i++) {
        if ((strcmp(argv[i], "-c") == 0 || strcmp(argv[i], "--config") == 0) && i + 1 < argc) {
            config_load(argv[i + 1], &cfg);
            break;
        }
    }

    // 命令行参数覆盖
    int rc = config_apply_args(&cfg, argc, argv);
    if (rc == -2) {
        usage(argv[0]);
        return 0;
    }
    if (rc != 0) {
        usage(argv[0]);
        return 1;
    }

    log_init(LOG_LEVEL_INFO);
    LOG_INFO("=== IoT broker (P1+P4/P5/P6) ===");
    LOG_INFO("mqtt_port=%d http_port=%d workers=%d backlog=%d",
             cfg.mqtt_port, cfg.http_port, cfg.workers, cfg.backlog);

    /* -------- P6: 数据库初始化 -------- */
    LOG_INFO("database: %s@%s:%d/%s (pool=%d)",
             cfg.db.user, cfg.db.host, cfg.db.port, cfg.db.database, cfg.db.pool_size);
    
    if (db_pool_init(&cfg.db) != 0) {
        LOG_ERROR("db_pool_init failed, continuing without database");
        // 不退出，允许无数据库运行（桩模式）
    } else {
        data_writer_init();
        LOG_INFO("database connected, data_writer ready");
    }

    /* -------- P4: 业务层初始化 -------- */
    device_manager_init();
    shadow_manager_init();
    alarm_service_init();

    /* -------- P5: HTTP + REST 路由 -------- */
    mqtt_broker_init();
    router_init();
    router_register_rest_routes();

    if (http_server_start(cfg.http_port) != 0) {
        LOG_WARN("HTTP API start failed, continuing without it");
    }

    /* -------- SSE 实时推送 -------- */
    if (sse_handler_init() != 0) {
        LOG_WARN("SSE handler init failed, continuing without it");
    }

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = on_signal;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    signal(SIGPIPE, SIG_IGN);

    int listen_fd = make_listener(cfg.mqtt_port, cfg.backlog);
    if (listen_fd < 0) return 1;

    g_thread_pool = thread_pool_create(cfg.workers, 1024);
    if (!g_thread_pool) { close(listen_fd); return 1; }

    g_loop = event_loop_create(g_thread_pool);
    if (!g_loop) { close(listen_fd); thread_pool_destroy(g_thread_pool); return 1; }

    if (event_loop_set_listen(g_loop, listen_fd) != 0) {
        event_loop_destroy(g_loop);
        thread_pool_destroy(g_thread_pool);
        return 1;
    }

    int result = event_loop_run(g_loop);
    http_server_stop();

    LOG_INFO("shutdown: total_conns=%u rc=%d", g_loop->total_conns, result);
    event_loop_destroy(g_loop);
    thread_pool_destroy(g_thread_pool);
    g_thread_pool = NULL;

    /* -------- 清理 -------- */
    data_writer_shutdown();
    db_pool_shutdown();

    LOG_INFO("bye");
    return result == 0 ? 0 : 1;
}
