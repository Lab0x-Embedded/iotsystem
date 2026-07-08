/**
 * @file group_manager.h
 *
 * P4 设备分组 (树形) — 支持多级分组
 *
 * 设计:
 *   - 每个分组有 parent_id, 构成森林
 *   - 设备通过 group_id 挂载到叶节点
 *   - 支持按组批量操作 (下发指令/查询状态)
 */
#ifndef E2_GROUP_MANAGER_H
#define E2_GROUP_MANAGER_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GROUP_ID_LEN   65
#define GROUP_NAME_LEN 128
#define MAX_GROUPS     128

typedef struct {
    char group_id[GROUP_ID_LEN];
    char name[GROUP_NAME_LEN];
    char parent_id[GROUP_ID_LEN];   /* 空串 = 根节点 */
    int  device_count;
    uint64_t created_at;
} device_group_t;

int  group_manager_init(void);

/** 创建分组; 返回 0 成功, -1 失败. */
int  group_create(const char *group_id, const char *name, const char *parent_id);

/** 删除分组 (叶节点且 device_count==0). */
int  group_remove(const char *group_id);

/** 查找分组. */
const device_group_t *group_find(const char *group_id);

/** 列出所有根分组. */
int  group_list_roots(const device_group_t **out, int max_n);

/** 列出子分组. */
int  group_list_children(const char *parent_id, const device_group_t **out, int max_n);

/** 增加/减少设备计数. */
void group_inc_device(const char *group_id);
void group_dec_device(const char *group_id);

#ifdef __cplusplus
}
#endif

#endif /* E2_GROUP_MANAGER_H */
