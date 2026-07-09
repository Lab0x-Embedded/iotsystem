/**
 * @file config.h
 *
 * 配置文件加载
 */
#ifndef E2_CONFIG_H
#define E2_CONFIG_H

#include "data/db_pool.h"

#ifdef __cplusplus
extern "C" {
#endif

/** 应用配置 */
typedef struct {
    int mqtt_port;
    int http_port;
    int workers;
    int backlog;
    db_pool_config_t db;
} app_config_t;

/**
 * 加载配置文件.
 *  @param path  JSON配置文件路径
 *  @param cfg   输出配置
 *  @return 0 成功, -1 失败
 */
int config_load(const char *path, app_config_t *cfg);

/**
 * 从命令行参数覆盖配置.
 */
int config_apply_args(app_config_t *cfg, int argc, char **argv);

#ifdef __cplusplus
}
#endif

#endif /* E2_CONFIG_H */
