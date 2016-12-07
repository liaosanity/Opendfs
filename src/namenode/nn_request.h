#ifndef NN_REQUEST_H
#define NN_REQUEST_H

#include "dfs_types.h"
#include "dfs_conn.h"
#include "dfs_buffer.h"
#include "dfs_queue.h"
#include "dfs_task.h"

typedef struct nn_conn_s nn_conn_t;
typedef void (*nn_event_handler_pt)(nn_conn_t *);

enum 
{
    ST_CONNCECTED = 0,
    ST_DISCONNCECTED = 1   
};

struct nn_conn_s 
{
    conn_t              *connection;
    buffer_t            *in;
    buffer_t            *out;
    queue_t              out_task;
    nn_event_handler_pt  read_event_handler;
    nn_event_handler_pt  write_event_handler;
    int32_t              count;
    int32_t              slow;
    queue_t              free_task;
    pool_t              *mempool;
    event_t              ev_timer;
    int32_t              max_task;
    int32_t              state;
    char                 ipaddr[32];
    log_t               *log;
};

void nn_conn_init(conn_t *c);
void *nn_conn_get_task(nn_conn_t *mc);
void nn_conn_free_task(nn_conn_t *mc, queue_t *q);
int  nn_conn_is_connected(nn_conn_t *mc);
int  nn_conn_output(nn_conn_t *mc);
int  nn_conn_outtask(nn_conn_t *mc, task_t *t);
int  nn_conn_update_state(nn_conn_t *mc, int state);
int  nn_conn_get_state(nn_conn_t *mc);
void nn_conn_finalize(nn_conn_t *mc);

#endif

