#ifndef NN_NET_RESPONSE_HANDLER_H
#define NN_NET_RESPONSE_HANDLER_H

#include "nn_thread.h"
#include "nn_task_queue.h"
#include "dfs_task_codec.h"
#include "nn_request.h"

struct nn_wb_s 
{
    nn_conn_t    *mc;
    dfs_thread_t *thread;
};

typedef struct nn_wb_s nn_wb_t;

typedef struct wb_node_s
{
    task_queue_node_t qnode;
    nn_wb_t           wbt;
} wb_node_t;

int  write_back(task_queue_node_t *node);
void write_back_notice_call(void *data);
void net_response_handler(void *data);
void write_back_pack_queue(queue_t *q, int send);
int  trans_task(task_t *task, dfs_thread_t *thread);
int write_back_task(task_t* t);

#endif

