/**
 * @file main.c — Phase 1 entry
 */
#include "common/log.h"
#include "server/event_loop.h"
#include "server/connection.h"
#include "server/thread_pool.h"
#include "server/mqtt_broker.h"
#include "api/http_server.h"

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
    fprintf(stderr, "Usage: %s [--port P] [--workers N] [--backlog B]\n", argv0);
}

int main(int argc, char **argv) {
    int port     = 65080;
    int workers  = 4;
    int backlog  = 1024;

    static struct option opts[] = {
        {"port",    required_argument, 0, 'p'},
        {"workers", required_argument, 0, 'w'},
        {"backlog", required_argument, 0, 'b'},
        {"help",    no_argument,       0, 'h'},
    };
    int opt_i = 0, c;
    while ((c = getopt_long(argc, argv, "p:w:b:h", opts, &opt_i)) != -1) {
        switch (c) {
            case 'p': port    = atoi(optarg); break;
            case 'w': workers = atoi(optarg); break;
            case 'b': backlog = atoi(optarg); break;
            case 'h': usage(argv[0]); return 0;
            default:  usage(argv[0]); return 1;
        }
    }

    log_init(LOG_LEVEL_INFO);
    LOG_INFO("=== IoT broker (P1) ===");
    LOG_INFO("port=%d workers=%d backlog=%d", port, workers, backlog);
    mqtt_broker_init();
    if (http_server_start(8080) != 0) {
        LOG_WARN("HTTP API start failed, continuing without it");
    }

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = on_signal;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    signal(SIGPIPE, SIG_IGN);

    int listen_fd = make_listener(port, backlog);
    if (listen_fd < 0) return 1;

    thread_pool_t *pool = thread_pool_create(workers, 1024);
    if (!pool) { close(listen_fd); return 1; }

    g_loop = event_loop_create(pool);
    if (!g_loop) { close(listen_fd); thread_pool_destroy(pool); return 1; }

    if (event_loop_set_listen(g_loop, listen_fd) != 0) {
        event_loop_destroy(g_loop);
        thread_pool_destroy(pool);
        return 1;
    }

    int rc = event_loop_run(g_loop);
    http_server_stop();

    LOG_INFO("shutdown: total_conns=%u rc=%d", g_loop->total_conns, rc);
    event_loop_destroy(g_loop);
    thread_pool_destroy(pool);
    g_loop = NULL;
    LOG_INFO("bye");
    return rc == 0 ? 0 : 1;
}
