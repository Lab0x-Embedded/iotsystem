/**
 * @file auth_device.h
 *
 * P4 设备认证 (MySQL) — 三重凭证校验
 *
 * 设计:
 *   - client_id = product_key/device_id
 *   - password = HMAC-SHA256(token, device_secret)
 *   - 查询 device 表校验 product_key + secret
 *
 * 接口可被 mqtt_broker 调用以替换硬编码 AUTH_* 宏.
 */
#ifndef E2_AUTH_DEVICE_H
#define E2_AUTH_DEVICE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** 初始化认证模块 (连接池). */
int auth_device_init(void);

/**
 * 校验设备 CONNECT 凭证.
 *
 *  @param product_key   产品 key
 *  @param device_id     设备 id
 *  @param password      CONNECT.password 明文
 *  @return 0 认证通过, -1 失败
 */
int auth_device_verify(const char *product_key,
                       const char *device_id,
                       const char *password);

/** 反注册. */
void auth_device_shutdown(void);

#ifdef __cplusplus
}
#endif

#endif /* E2_AUTH_DEVICE_H */
