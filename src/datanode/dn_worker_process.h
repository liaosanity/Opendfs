#ifndef DN_WORKER_PROCESSS_H
#define DN_WORKER_PROCESSS_H

#include "dn_cycle.h"

void worker_processer(cycle_t *cycle, void *data);
void register_thread_initialized(void);
void register_thread_exit(void);

#endif

