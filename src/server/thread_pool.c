/**
 * @file thread_pool.c - Phase 1 stub
 *
 * The thread thread_pool infrastructure is exercised by a single no-op
 * submission per connection-accept.  In Phase 1 we intentionally do
 * the echo inline in the event thread to keep the line count down;
 * the thread-pool machinery is here so Phase 2 can move the heavy
 * lifting without restructuring code.
 */

#include "thread_pool.h"
#include "common/log.h"

#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>

#define QUEUE_CAP 256

typedef struct task { task_fn_t fn; void *arg; } task_t;

struct thread_pool {
    pthread_mutex_t mtx;
    pthread_cond_t  work;
    pthread_cond_t  space;
    pthread_cond_t  idle;
    task_t queue[QUEUE_CAP];
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
        p->head = (p->head + 1) % QUEUE_CAP;
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
    (void)queue_cap;
    thread_pool_t *p = calloc(1, sizeof(*p));
    if (!p) return NULL;
    p->tids = calloc(worker_count, sizeof(pthread_t));
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
    while (p->count == QUEUE_CAP && !p->stop)
        pthread_cond_wait(&p->space, &p->mtx);
    if (p->stop) { pthread_mutex_unlock(&p->mtx); return -1; }
    p->queue[p->tail] = (task_t){fn, arg};
    p->tail = (p->tail + 1) % QUEUE_CAP;
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
    free(p->tids);
    free(p);
}

int thread_pool_pending(const thread_pool_t *p) {
    pthread_mutex_lock((pthread_mutex_t *)&p->mtx);
    int c = p->count + p->active;
    pthread_mutex_unlock((pthread_mutex_t *)&p->mtx);
    return c;
}
