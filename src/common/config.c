/**
 * @file config.c
 *
 * 配置文件加载 — cJSON 实现
 *
 * 数据库字符串字段使用 strdup 深拷贝，避免静态定长 buffer 截断；
 * 由 config_free 负责释放（幂等，可重复调用）。
 */
#include "common/config.h"
#include "common/log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <getopt.h>
#include <cjson/cJSON.h>

/** 读取文件到字符串 */
static char *read_file(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = (char *)malloc((size_t)len + 1);
    if (buf) {
        fread(buf, 1, len, f);
        buf[len] = '\0';
    }
    fclose(f);
    return buf;
}

/** 标记 db 字符串当前是否为堆分配（由 strdup 产生） */
static bool g_db_strings_owned = false;

void config_free(app_config_t *cfg) {
    if (!cfg) return;
    if (g_db_strings_owned) {
        free((void *)cfg->db.host);
        free((void *)cfg->db.user);
        free((void *)cfg->db.password);
        free((void *)cfg->db.database);
        g_db_strings_owned = false;
    }
    cfg->db.host = cfg->db.user = cfg->db.password = cfg->db.database = NULL;
}

/** 用 strdup 安全替换一个字符串字段；若已拥原则释放旧值 */
static void set_str_field(const char **dst, const char *src) {
    if (!dst || !src) return;
    if (g_db_strings_owned)
        free((void *)*dst);
    char *dup = strdup(src);
    if (dup)
        *dst = dup;
}

int config_load(const char *path, app_config_t *cfg) {
    if (!path || !cfg) return -1;

    /* 释放上一次 load 可能遗留的堆字符串，避免重复 load 泄漏 */
    config_free(cfg);

    // 默认值（全部 strdup，因此 g_db_strings_owned 随后设为 true）
    cfg->mqtt_port = 65080;
    cfg->http_port = 8080;
    cfg->workers = 4;
    cfg->backlog = 1024;
    set_str_field(&cfg->db.host, "127.0.0.1");
    set_str_field(&cfg->db.user, "root");
    set_str_field(&cfg->db.password, "your_password");
    set_str_field(&cfg->db.database, "e2_iot");
    cfg->db.port = 3306;
    cfg->db.pool_size = 4;
    g_db_strings_owned = true;

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

    // 数据库配置（strdup 覆盖默认值）
    cJSON *db = cJSON_GetObjectItem(root, "database");
    if (db) {
        if ((item = cJSON_GetObjectItem(db, "host")))
            set_str_field(&cfg->db.host, item->valuestring);
        if ((item = cJSON_GetObjectItem(db, "port")))
            cfg->db.port = (uint16_t)item->valueint;
        if ((item = cJSON_GetObjectItem(db, "user")))
            set_str_field(&cfg->db.user, item->valuestring);
        if ((item = cJSON_GetObjectItem(db, "password")))
            set_str_field(&cfg->db.password, item->valuestring);
        if ((item = cJSON_GetObjectItem(db, "database")))
            set_str_field(&cfg->db.database, item->valuestring);
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
            case 'D': set_str_field(&cfg->db.host, optarg); break;
            case 'P': cfg->db.port = (uint16_t)atoi(optarg); break;
            case 'U': set_str_field(&cfg->db.user, optarg); break;
            case 'W': set_str_field(&cfg->db.password, optarg); break;
            case 'N': set_str_field(&cfg->db.database, optarg); break;
            case 'h': return -2;
            default: return -1;
        }
    }
    return 0;
}
