/**
 * @file mqtt_topic.h  — MQTT 3.1.1 topic 匹配
 */
#ifndef E2_MQTT_TOPIC_H
#define E2_MQTT_TOPIC_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * 检查订阅 topic 是否匹配发布 topic.
 *
 * 支持:
 *   - 精确匹配:  "a/b" == "a/b"
 *   - 单级通配符: "a/+" 匹配 "a/x" 但不匹配 "a/x/y"
 *   - 多级通配符: "a/#" 匹配 "a/x" "a/x/y" 以及 "a/" 和 "a"
 *
 * 返回 1 匹配, 0 不匹配.
 */
int mqtt_topic_match(const char *sub_topic, const char *pub_topic);

/**
 * 校验 topic 是否合法 (MQTT-4.7.3).
 *   - 不允许空 topic (除非是 # 订阅)
 *   - 不允许包含通配符的 PUBLISH
 *   - 不允许 # 不在末尾
 *   - 不允许 + 不占用完整一级
 *
 * 返回 1 合法, 0 不合法.
 */
int mqtt_topic_valid(const char *topic, int is_subscribe);

#ifdef __cplusplus
}
#endif

#endif /* E2_MQTT_TOPIC_H */
