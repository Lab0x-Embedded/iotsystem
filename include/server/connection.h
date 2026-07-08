/**
 * @file connection.h
 */
#ifndef E2_CONNECTION_H
#define E2_CONNECTION_H

#include <stddef.h>
#include <stdint.h>
#include <time.h>

typedef struct connection {
    int fd;
    time_t last_active;
    void *wrapper;       /* back-pointer to conn_wrapper_t, used by event loop */
    unsigned char *rx_buf; size_t rx_off, rx_len, rx_cap;
    unsigned char *tx_buf; size_t tx_len, tx_cap;
} connection_t;

connection_t *connection_create(int fd);
void          connection_destroy(connection_t *c);
int  connection_read(connection_t *c);
int  connection_flush_tx(connection_t *c);
void connection_echo_run(connection_t *c);
#endif
