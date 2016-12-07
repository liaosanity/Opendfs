#ifndef FAIO_WORKER_H
#define FAIO_WORKER_H

#include "faio_manager.h"
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <limits.h>
#include <pthread.h>
#include <errno.h>
#include "faio_thread.h"

int faio_worker_manager_release(faio_worker_manager_t *worker_mgr, 
    faio_errno_t *err);
int faio_worker_manager_init(faio_manager_t *faio_mgr, 
    faio_worker_properties_t *properties, faio_errno_t *err);
int faio_maybe_start_thread(faio_worker_manager_t *worker_mgr, 
	faio_errno_t *err);
int faio_worker_set_max_idle(faio_worker_manager_t *worker_mgr, 
    unsigned int max_idle, faio_errno_t *err);
int faio_worker_set_max_threads(faio_worker_manager_t *worker_mgr, 
    unsigned int max_threads, faio_errno_t *err);
int faio_worker_set_idle_timeout(faio_worker_manager_t *worker_mgr, 
    unsigned int idle_timeout, faio_errno_t *err);
int faio_worker_maybe_start_thread(faio_worker_manager_t *worker_mgr, 
    faio_errno_t *err);

#endif

