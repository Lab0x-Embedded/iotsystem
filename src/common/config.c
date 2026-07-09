/**
 * @file config.c
 *
 * 配置文件加载 — cJSON 实现
 */
#include "common/config.h"
#include "common/log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <cjson/cJSON.h>

/** 读取文件到字符串 */
static char *read_file(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = (char *)malloc(len + 1);
    if (buf) {
        fread(buf, 1, len, f);
        buf[len] = '\0';
    }
    fclose(f);
    return buf;
}

int config_load(const char *path, app_config_t *cfg) {
    if (!path || !cfg) return -1;

    // 默认值
    cfg->mqtt_port = 65080;
    cfg->http_port = 8080;
    cfg->workers = 4;
    cfg->backlog = 1024;
    cfg->db.host = "127.0.0.1";
    cfg->db.port = 3306;
    cfg->db.user = "root";
    cfg->db.password = "your_password";
    cfg->db.database = "e2_iot";
    cfg->db.pool_size = 4;

    char *json_str = read_file(path);
    if (!json_str) {
        LOG_WARN("config: cannot read %s, using defaults", path);
        return 0;  // 使用默认值，不算失败
    }

    cJSON *root = cJSON_Parse(json_str);
    free(json_str);
    if (!root) {
        LOG_ERROR("config: JSON parse error in %s", path);
        return -1;
    }

    // 顶层配置
    cJSON *item;
    if ((item = cJSON_GetObjectItem(root, "mqtt_port")))
        cfg->mqtt_port = item->valueint;
    if ((item = cJSON_GetObjectItem(root, "http_port")))
        cfg->http_port = item->valueint;
    if ((item = cJSON_GetObjectItem(root, "workers")))
        cfg->workers = item->valueint;
    if ((item = cJSON_GetObjectItem(root, "backlog")))
        cfg->backlog = item->valueint;

    // 数据库配置
    cJSON *db = cJSON_GetObjectItem(root, "database");
    if (db) {
        // host/user/password/database 需要复制，因为cJSON生命周期问题
        static char db_host[64] = "127.0.0.1";
        static char db_user[64] = "root";
        static char db_pass[128] = "your_password";
        static char db_name[64] = "e2_iot";
        
        if ((item = cJSON_GetObjectItem(db, "host"))) {
            strncpy(db_host, item->valuestring, sizeof(db_host) - 1);
            cfg->db.host = db_host;
        }
        if ((item = cJSON_GetObjectItem(db, "port")))
            cfg->db.port = (uint16_t)item->valueint;
        if ((item = cJSON_GetObjectItem(db, "user"))) {
            strncpy(db_user, item->valuestring, sizeof(db_user) - 1);
            cfg->db.user = db_user;
        }
        if ((item = cJSON_GetObjectItem(db, "password"))) {
            strncpy(db_pass, item->valuestring, sizeof(db_pass) - 1);
            cfg->db.password = db_pass;
        }
        if ((item = cJSON_GetObjectItem(db, "database"))) {
            strncpy(db_name, item->valuestring, sizeof(db_name) - 1);
            cfg->db.database = db_name;
        }
        if ((item = cJSON_GetObjectItem(db, "pool_size")))
            cfg->db.pool_size = item->valueint;
    }

    cJSON_Delete(root);
    LOG_INFO("config: loaded from %s", path);
    return 0;
}

int config_apply_args(app_config_t *cfg, int argc, char **argv) {
    static struct option opts[] = {
        {"port",        required_argument, 0, 'p'},
        {"http",        required_argument, 0, 'H'},
        {"workers",     required_argument, 0, 'w'},
        {"backlog",     required_argument, 0, 'b'},
        {"config",      required_argument, 0, 'c'},
        {"db-host",     required_argument, 0, 'D'},
        {"db-port",     required_argument, 0, 'P'},
        {"db-user",     required_argument, 0, 'U'},
        {"db-password", required_argument, 0, 'W'},
        {"db-name",     required_argument, 0, 'N'},
        {"help",        no_argument,       0, 'h'},
        {0, 0, 0, 0}
    };
    int opt_i = 0, c;
    optind = 1;  // 重置 getopt

    while ((c = getopt_long(argc, argv, "p:H:w:b:c:h", opts, &opt_i)) != -1) {
        switch (c) {
            case 'p': cfg->mqtt_port = atoi(optarg); break;
            case 'H': cfg->http_port = atoi(optarg); break;
            case 'w': cfg->workers = atoi(optarg); break;
            case 'b': cfg->backlog = atoi(optarg); break;
            case 'c': config_load(optarg, cfg); break;
            case 'D': cfg->db.host = optarg; break;
            case 'P': cfg->db.port = (uint16_t)atoi(optarg); break;
            case 'U': cfg->db.user = optarg; break;
            case 'W': cfg->db.password = optarg; break;
            case 'N': cfg->db.database = optarg; break;
            case 'h': return -2;  // 显示帮助
            default: return -1;
        }
    }
    return 0;
}
