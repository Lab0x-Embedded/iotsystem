/**
 * @file thing_model.h
 *
 * 轻量物模型 — 产品级属性白名单
 *
 * 设计：
 *   - product_properties 表定义每个产品允许上报的属性（identifier + 类型）
 *   - 上报校验（publish_worker 单一收口，OneNET params 与原生 datapoints
 *     两条链路都经过）：
 *       · 产品未定义任何属性 = 自由模式，全部放行（兼容既有演示设备）
 *       · 定义了属性后，白名单外的 identifier 拒绝（TM_UNKNOWN，对齐
 *         OneNET 10411 identifier not exist 语义）
 *       · bool 类型要求值为 0/1，否则 TM_TYPE_MISMATCH
 *   - 产品级内存缓存（30s TTL）+ 增删时主动失效，避免每条数据点查库
 */
#ifndef E2_THING_MODEL_H
#define E2_THING_MODEL_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    TM_OK = 0,          /* 白名单命中，允许 */
    TM_FREE = 3,        /* 该产品未定义任何属性 → 自由模式，放行 */
    TM_UNKNOWN = 1,     /* identifier 不在白名单 */
    TM_TYPE_MISMATCH = 2/* 类型不符（bool 要求 0/1） */
} tm_check_t;

/** 启动时调用：自动建表（幂等）。DB 不可用时告警但不阻塞启动。 */
int thing_model_init(void);

/** 上报校验。自管连接（内部短时从连接池取一条）。
 *  未持有连接的调用方用这个（如 OneNET REST 同步链路）。 */
tm_check_t thing_model_check(const char *product_key,
                             const char *identifier, double value);

struct db_conn;
/** 上报校验（调用方已持有连接）：复用传入连接加载白名单缓存。
 *  publish_worker 必须用这个 —— 上报路径已持有一条池连接, 若再走
 *  thing_model_check 会嵌套占用第二条, 高并发时把池压满。 */
tm_check_t thing_model_check_with_conn(struct db_conn *conn,
                                       const char *product_key,
                                       const char *identifier, double value);

/* ---- REST 管理接口（handler_product 用） ---- */

/** 添加属性（已存在则更新类型/描述）。0 成功，-1 失败。 */
int thing_model_add(const char *product_key, const char *identifier,
                    const char *prop_type, const char *description);

/** 删除属性。受影响行数（0 = 不存在）。 */
int thing_model_delete(const char *product_key, const char *identifier);

/** 列出产品的全部属性。返回条数，-1 失败；rows[i] = "identifier|type|desc"。 */
int thing_model_list(const char *product_key,
                     char rows[][256], int max_rows);

#ifdef __cplusplus
}
#endif

#endif /* E2_THING_MODEL_H */
