/**
 * @file thread_pool.h
 */
#ifndef E2_THREAD_POOL_H
#define E2_THREAD_POOL_H

typedef void (*task_fn_t)(void *arg);
typedef struct thread_pool thread_pool_t;
thread_pool_t *thread_pool_create(int worker_count, int queue_cap);
int  thread_pool_submit(thread_pool_t *pool, task_fn_t fn, void *arg);
void thread_pool_destroy(thread_pool_t *pool);
int  thread_pool_pending(const thread_pool_t *p);
#endif
