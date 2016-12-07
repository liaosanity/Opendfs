#ifndef NN_PAXOS_H
#define NN_PAXOS_H

#include "nn_cycle.h"
#include "nn_file_index.h"

int nn_paxos_worker_init(cycle_t *cycle);
int nn_paxos_worker_release(cycle_t *cycle);
int nn_paxos_run();
void set_checkpoint_instanceID(const uint64_t llInstanceID);
void do_paxos_task_handler(void *q);
int check_traverse(uchar_t *path, task_t *task, 
	fi_inode_t *finodes[], int num);
int check_ancestor_access(uchar_t *path, task_t *task, 
	short access, fi_inode_t *finode);

#endif
