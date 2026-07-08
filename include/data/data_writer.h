/**
 * @file data_writer.h
 *
 * P5 批量异步写入 — 缓冲队列 + 定时刷盘
 *
 * 设计:
 *   - 上报数据入队 (无锁或有锁 ring buffer)
 *   - 缓冲区满或定时 1s 触发批量 INSERT
 *   - 单条 INSERT → 批量 multi-value INSERT
 */
#ifndef E2_DATA_WRITER_H
#define E2_DATA_WRITER_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

int  data_writer_init(void);
void data_writer_shutdown(void);

/**
 * 入队一条设备上报.
 *  @param device_id  设备 id
 *  @param metric     指标名
 *  @param value      数值
 *  @param ts         时间戳
 *  @return 0 成功, -1 失败
 */
int data_writer_enqueue(const char *device_id, const char *metric,
                        double value, uint64_t ts);

/** 强制刷盘. */
void data_writer_flush(void);

/** 当前缓冲区深度. */
int  data_writer_pending(void);

#ifdef __cplusplus
}
#endif

#endif /* E2_DATA_WRITER_H */
