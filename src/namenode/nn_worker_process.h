#ifndef NN_WORKER_PROCESSS_H
#define NN_WORKER_PROCESSS_H

#include "nn_cycle.h"

void worker_processer(cycle_t *cycle, void *data);
void register_thread_initialized(void);
void dispatch_task(void *);
void register_thread_exit(void);

#endif

