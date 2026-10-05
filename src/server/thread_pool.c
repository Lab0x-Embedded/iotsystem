/**
 * @file thread_pool.c - 通用线程池
 *
 * 有界任务队列 + 固定 worker 线程。队列容量由 thread_pool_create 的
 * queue_cap 参数决定（动态分配），提交在队列满时阻塞（背压，不丢弃）。
 */

#include "thread_pool.h"
#include "common/log.h"

#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>

typedef struct task { task_fn_t fn; void *arg; } task_t;

struct thread_pool {
    pthread_mutex_t mtx;
    pthread_cond_t  work;
    pthread_cond_t  space;
    pthread_cond_t  idle;
    task_t *queue;
    int queue_cap;
    int head, tail, count;
    int worker_count;
    int stop;
    int active;
    pthread_t *tids;
};

static void *worker_entry(void *arg) {
    thread_pool_t *p = arg;
    for (;;) {
        pthread_mutex_lock(&p->mtx);
        while (p->count == 0 && !p->stop)
            pthread_cond_wait(&p->work, &p->mtx);
        if (p->stop && p->count == 0) {
            pthread_mutex_unlock(&p->mtx);
            break;
        }
        task_t t = p->queue[p->head];
        p->head = (p->head + 1) % p->queue_cap;
        p->count--;
        p->active++;
        pthread_cond_signal(&p->space);
        pthread_mutex_unlock(&p->mtx);
        t.fn(t.arg);
        pthread_mutex_lock(&p->mtx);
        p->active--;
        if (p->active == 0 && p->count == 0 && p->stop)
            pthread_cond_signal(&p->idle);
        pthread_mutex_unlock(&p->mtx);
    }
    return NULL;
}

thread_pool_t *thread_pool_create(int worker_count, int queue_cap) {
    if (worker_count <= 0 || queue_cap <= 0)
        return NULL;

    thread_pool_t *p = calloc(1, sizeof(*p));
    if (!p) return NULL;

    p->tids = calloc((size_t)worker_count, sizeof(pthread_t));
    if (!p->tids) { free(p); return NULL; }

    p->queue = calloc((size_t)queue_cap, sizeof(task_t));
    if (!p->queue) { free(p->tids); free(p); return NULL; }

    p->queue_cap = queue_cap;
    p->worker_count = worker_count;

    pthread_mutex_init(&p->mtx, NULL);
    pthread_cond_init(&p->work, NULL);
    pthread_cond_init(&p->space, NULL);
    pthread_cond_init(&p->idle, NULL);

    for (int i = 0; i < worker_count; i++)
        pthread_create(&p->tids[i], NULL, worker_entry, p);

    return p;
}

int thread_pool_submit(thread_pool_t *p, task_fn_t fn, void *arg) {
    pthread_mutex_lock(&p->mtx);
    while (p->count == p->queue_cap && !p->stop)
        pthread_cond_wait(&p->space, &p->mtx);
    if (p->stop) { pthread_mutex_unlock(&p->mtx); return -1; }
    p->queue[p->tail] = (task_t){fn, arg};
    p->tail = (p->tail + 1) % p->queue_cap;
    p->count++;
    pthread_cond_signal(&p->work);
    pthread_mutex_unlock(&p->mtx);
    return 0;
}

void thread_pool_destroy(thread_pool_t *p) {
    if (!p) return;
    pthread_mutex_lock(&p->mtx);
    p->stop = 1;
    pthread_cond_broadcast(&p->work);
    pthread_mutex_unlock(&p->mtx);
    for (int i = 0; i < p->worker_count; i++)
        pthread_join(p->tids[i], NULL);
    free(p->queue);
    free(p->tids);
    free(p);
}

int thread_pool_pending(const thread_pool_t *p) {
    pthread_mutex_lock((pthread_mutex_t *)&p->mtx);
    int c = p->count + p->active;
    pthread_mutex_unlock((pthread_mutex_t *)&p->mtx);
    return c;
}
