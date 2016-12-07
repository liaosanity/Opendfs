#ifndef FAIO_MANAGER_H
#define FAIO_MANAGER_H

#include <pthread.h>
#include <stdint.h>
#include "faio_thread.h"
#include "faio_queue.h"
#include "faio_error.h"
#include "faio_atomic_lock.h"

typedef enum 
{
    FAIO_IO_TYPE_READ,
    FAIO_IO_TYPE_WRITE,
    FAIO_IO_TYPE_SENDFILE,
    FAIO_IO_TYPE_END
} FAIO_IO_TYPE;

typedef enum 
{
    FAIO_STATE_WAIT,
    FAIO_STATE_DOING,
    FAIO_STATE_DONE,
    FAIO_STATE_CANCEL,
    FAIO_STATE_END
} FAIO_TASK_STATE;

#define     FAIO_OK                 0
#define     FAIO_ERROR             -1
#define     FAIO_TRUE               1
#define     FAIO_FALSE              0
#define     DEFAULT_QUE_SIZE        1024

typedef struct faio_atomic_s                faio_atomic_t;
typedef struct faio_data_task_s             faio_data_task_t;
typedef struct faio_data_queue_s            faio_data_queue_t;
typedef struct faio_manager_s               faio_manager_t;
typedef struct faio_data_manager_s          faio_data_manager_t;
typedef struct faio_notifier_manager_s      faio_notifier_manager_t;
typedef struct faio_handler_manager_s       faio_handler_manager_t;
typedef struct faio_worker_manager_s        faio_worker_manager_t;
typedef struct faio_worker_thread_s         faio_worker_thread_t;
typedef struct faio_worker_properties_s     faio_worker_properties_t;
typedef faio_worker_properties_t            faio_properties_t;

typedef int (*faio_io_handler_t) (faio_data_task_t *task);
typedef void (*faio_callback_t) (faio_data_task_t *task);

struct faio_atomic_s 
{
    uint64_t     value;
};

struct faio_data_task_s 
{
    faio_data_task_t        *next;
    FAIO_IO_TYPE             io_type;
    faio_callback_t          io_callback;
    faio_notifier_manager_t *notifier;
    faio_task_errno_t        err;
    int                      cancel_flag;
    int                      state;
};

struct faio_data_queue_s 
{
    faio_data_task_t        *queue_start;
    faio_data_task_t        *queue_end;
    faio_atomic_lock_t       lock;
    size_t                   size;
};

struct faio_data_manager_s 
{
    faio_manager_t          *faio_mgr;
    faio_data_queue_t        req_queue;
    faio_cond_t              req_wait;
    unsigned int             max_size;
};

struct faio_handler_manager_s 
{
    faio_manager_t          *faio_mgr;
    faio_io_handler_t        handler[FAIO_IO_TYPE_END];
};

struct faio_notifier_manager_s 
{
    int                 nfd;
    faio_manager_t     *manager;
    faio_atomic_t       count;
    faio_condition_t    cond;    
    faio_atomic_t       noticed;
    int                 release;
};

struct faio_worker_thread_s 
{
    faio_queue_t                q;
    faio_worker_t               tid;
    faio_worker_manager_t      *worker_mgr;
};

struct faio_worker_properties_s 
{
    unsigned int                max_idle;
    unsigned int                idle_timeout;
    unsigned int                max_thread;
    unsigned int                pre_start; 
};

struct faio_worker_manager_s 
{
    unsigned int                idle;
    unsigned int                started;
    unsigned int                want_quit;
    faio_queue_t                worker_queue;
    faio_worker_properties_t    worker_properties;
    faio_mutex_t                work_lock;
    faio_cond_t                 worker_wait;
    faio_cond_t                 quit_wait;
    faio_data_manager_t        *data_mgr;
    faio_handler_manager_t     *handler_mgr;
    int                         init_flag;
};

struct faio_manager_s 
{
    faio_data_manager_t         data_manager;
    faio_worker_manager_t       worker_manager;
    faio_handler_manager_t      handler_manager;
    int                         release_flag;
};

int faio_manager_init(faio_manager_t *faio_mgr, 
	faio_properties_t *faio_prop, unsigned int max_task_num, 
	faio_errno_t *error);
int faio_manager_release(faio_manager_t *faio_mgr, faio_errno_t *error);
int faio_notifier_init(faio_notifier_manager_t *notifier_mgr, 
    faio_manager_t *faio_mgr, faio_errno_t *error);
int faio_notifier_release(faio_notifier_manager_t *notifier_mgr, 
    faio_errno_t *error);
int faio_register_handler(faio_manager_t *faio_mgr, faio_io_handler_t handler, 
    FAIO_IO_TYPE io_type, faio_errno_t *error);
int faio_read(faio_notifier_manager_t *notifier_mgr, 
	faio_callback_t faio_callback, faio_data_task_t *task, faio_errno_t *error);
int faio_write(faio_notifier_manager_t *notifier_mgr, 
	faio_callback_t faio_callback, faio_data_task_t *task, faio_errno_t *error);
int faio_sendfile(faio_notifier_manager_t *notifier_mgr, 
	faio_callback_t faio_callback, faio_data_task_t *task, faio_errno_t *error);
int faio_recv_notifier(faio_notifier_manager_t *notifier_mgr, 
	faio_errno_t *error);
int faio_remove_task(faio_data_task_t *task, faio_errno_t *error);
int faio_set_max_idle(faio_manager_t *faio_mgr, unsigned int max_idle, 
	faio_errno_t *error);
int faio_set_max_threads(faio_manager_t *faio_mgr, 
	unsigned int max_threads, faio_errno_t *error);
int faio_set_idle_timeout(faio_manager_t *faio_mgr, unsigned int idle_timeout,
    faio_errno_t *error);
int faio_get_task_errno(faio_data_task_t *task, int *err, int *sys, 
    faio_errno_t *error);
const char *faio_get_error_msg(faio_errno_t *err_no);

#endif

