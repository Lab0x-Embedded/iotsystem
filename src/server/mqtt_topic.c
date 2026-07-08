/**
 * @file mqtt_topic.c  — MQTT 3.1.1 topic 匹配实现
 */
#include "mqtt_topic.h"
#include <string.h>

/* ------------------------------------------------------------------ */
/* topic 匹配: 递归分割路径, 逐级比较                                   */
/* ------------------------------------------------------------------ */

/* 在 buf[0..len) 中找 '/' 分隔符; 返回位置或 -1 */
static int find_slash(const char *buf, int len) {
    for (int i = 0; i < len; i++)
        if (buf[i] == '/') return i;
    return -1;
}

int mqtt_topic_match(const char *sub_topic, const char *pub_topic) {
    if (!sub_topic || !pub_topic) return 0;

    int sub_len = (int)strlen(sub_topic);
    int pub_len = (int)strlen(pub_topic);

    /* 多级通配符 #: sub_topic 为 "#" 匹配任何 topic */
    if (sub_len == 1 && sub_topic[0] == '#') return 1;

    int si = 0, pi = 0;
    while (si < sub_len && pi < pub_len) {
        /* 找当前级别的边界 */
        int s_slash = find_slash(sub_topic + si, sub_len - si);
        int p_slash = find_slash(pub_topic + pi, pub_len - pi);

        int s_seg_len = (s_slash >= 0) ? s_slash : (sub_len - si);
        int p_seg_len = (p_slash >= 0) ? p_slash : (pub_len - pi);

        const char *s_seg = sub_topic + si;
        const char *p_seg = pub_topic + pi;

        /* 多级通配符 #: 必须独自占一级 */
        if (s_seg_len == 1 && s_seg[0] == '#') {
            return 1;  /* # 匹配剩下的所有级别 */
        }

        /* 单级通配符 +: 匹配任意一级, 但不跨级 */
        if (s_seg_len == 1 && s_seg[0] == '+') {
            /* + 匹配当前 p_seg; 继续下一级 */
        } else {
            /* 精确匹配当前级别 */
            if (s_seg_len != p_seg_len || memcmp(s_seg, p_seg, s_seg_len) != 0)
                return 0;
        }

        /* 跳到下一级 */
        si += s_seg_len;
        pi += p_seg_len;
        if (si < sub_len) { si++; }  /* 跳过 '/' */
        if (pi < pub_len) { pi++; }
    }

    /* 如果 sub 还有剩余且是 #, 匹配;
     * 如果 sub 和 pub 都跑完了, 匹配;
     * 否则 sub 或 pub 有多余级别 → 不匹配 */
    if (si < sub_len) {
        /* sub 剩下的只有 "#" 才匹配 */
        return (sub_len - si == 1 && sub_topic[si] == '#');
    }
    return (si >= sub_len && pi >= pub_len);
}

/* ------------------------------------------------------------------ */
/* topic 合法性校验                                                     */
/* ------------------------------------------------------------------ */
int mqtt_topic_valid(const char *topic, int is_subscribe) {
    if (!topic || topic[0] == '\0') return 0;

    int len = (int)strlen(topic);

    /* 多级通配符 # 必须在末尾且独占一级 */
    if (topic[len - 1] == '#') {
        if (!is_subscribe) return 0;  /* PUBLISH 不允许 # */
        if (len > 1 && topic[len - 2] != '/') return 0;
    }

    /* 单级通配符 + 必须是完整一级 */
    if (!is_subscribe) {
        /* PUBLISH 不允许任何通配符 */
        if (strchr(topic, '+') || strchr(topic, '#')) return 0;
    }

    for (int i = 0; i < len; i++) {
        if (topic[i] == '#') {
            if (i != len - 1) return 0;
        }
        if (topic[i] == '+') {
            /* + 必须是独立一级: 前一个字符是 '/' 或开头, 后一个字符是 '/' 或结尾 */
            int prev_ok = (i == 0 || topic[i - 1] == '/');
            int next_ok = (i == len - 1 || topic[i + 1] == '/');
            if (!prev_ok || !next_ok) return 0;
        }
    }

    return 1;
}
