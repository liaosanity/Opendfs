#ifndef NN_RPC_SERVER_H
#define NN_RPC_SERVER_H

#include "nn_cycle.h"
#include "dfs_task.h"

int nn_rpc_worker_init(cycle_t *cycle);
int nn_rpc_worker_release(cycle_t *cycle);

int nn_rpc_service_run(task_t *task);

#endif 

