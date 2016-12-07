#ifndef NN_ERROR_LOG_H
#define NN_ERROR_LOG_H

#include "nn_cycle.h"

int nn_error_log_init(cycle_t *cycle);
int nn_error_log_release(cycle_t *cycle);
void nn_log_paxos(const int level, const char *fmt, va_list args);

#endif

