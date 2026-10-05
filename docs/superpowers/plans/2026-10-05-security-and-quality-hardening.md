# 安全加固与代码质量优化 Implementation Plan

> **For agentic workers:** 按 task 顺序执行，每步用 checkbox (`- [ ]`) 跟踪；改动独立可验证，小步提交。

**Goal:** 消除硬编码凭证、修复 thread_pool 队列容量 bug、提升关键文件可读性、补齐 .gitignore。

**Architecture:** 纯 C 语言重构 + 配置分离，不引入新第三方依赖，不改变对外 API 与行为。

**Tech Stack:** C11、Make/CMake、git。

## Global Constraints

- C11，`-Wall -Wextra -Werror`（不引入新警告）。
- 不新增第三方依赖。
- 不改变 REST / MQTT 对外协议与配置项语义（仅新增）。
- 每个 Task 结束必须 `make build` 通过。

---

### Task 1: 分离敏感配置

**Files:** Move `deploy/config.json` -> `deploy/config.example.json`; Modify `.gitignore`

- [ ] 复制 config.json 为 config.example.json，密码改 `CHANGE_ME`
- [ ] `rm deploy/config.json`，.gitignore 追加 `deploy/config.json`
- [ ] README 配置小节补充 `cp` 说明
- [ ] 验证 `git status --short`
- [ ] 提交

### Task 2: 修复 thread_pool 忽略 queue_cap

**Files:** Modify `src/server/thread_pool.c`

- [ ] `task_t queue[QUEUE_CAP]` -> `task_t *queue` + `int queue_cap`
- [ ] `thread_pool_create` 动态分配 queue，校验参数
- [ ] 替换所有 `QUEUE_CAP` 为 `p->queue_cap`
- [ ] `thread_pool_destroy` 释放 `p->queue`
- [ ] 删除 `#define QUEUE_CAP 256`
- [ ] `make build` 并提交

### Task 3: 重构 log.c 可读性

**Files:** Modify `src/common/log.c`

- [ ] 展开压缩的单行函数（`ls` -> `level_str` 等，逻辑不变）
- [ ] `make build` 并提交

### Task 4: config.c 深拷贝数据库配置

**Files:** Modify `src/common/config.c`, `include/common/config.h`

- [ ] config.h 声明 `void config_free(app_config_t *cfg);`
- [ ] 静态 buffer -> `strdup` 深拷贝
- [ ] 实现 `config_free`
- [ ] `make build` 并提交

### Task 5: 补齐 .gitignore

**Files:** Modify `.gitignore`

- [ ] 追加 `client/build/` `client/.qtcreator/` `*.app` `*.user` `compile_commands.json`
- [ ] 提交

### Task 6: 数据库密码支持环境变量

**Files:** Modify `src/main.c`, `README.md`

- [ ] 移除占位默认密码 `your_password` -> 空串
- [ ] 空密码时读 `E2_DB_PASSWORD` 环境变量
- [ ] 更新 usage() 与 README
- [ ] `make build` 并提交
