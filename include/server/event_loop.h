/**
 * @file event_loop.h
 */
#ifndef E2_EVENT_LOOP_H
#define E2_EVENT_LOOP_H

#include <stdint.h>
#include "connection.h"
#include "mqtt_connection.h"
#include "thread_pool.h"

typedef struct event_loop {
    int kq;
    int listen_fd;
    int sp[2];
    void *events;
    thread_pool_t *pool;
    uint32_t active_conns;
    uint32_t total_conns;
    volatile int running;
} event_loop_t;

#define MAX_EVENTS 1024

event_loop_t *event_loop_create(thread_pool_t *pool);
int           event_loop_set_listen(event_loop_t *loop, int listen_fd);
int           event_loop_add_conn(event_loop_t *loop, connection_t *c);
int           event_loop_mod_in(event_loop_t *loop, connection_t *c);
int           event_loop_del_conn(event_loop_t *loop, connection_t *c);
int           event_loop_run(event_loop_t *loop);
void          event_loop_stop(event_loop_t *loop);
void          event_loop_destroy(event_loop_t *loop);

#endif /* E2_EVENT_LOOP_H */
