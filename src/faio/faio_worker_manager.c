#include "faio_worker_manager.h"
#include "faio_thread.h"
#include "faio_manager.h"
#include "faio_data_manager.h"
#include "faio_handler_manager.h"
#include "faio_notifier_manager.h"
#include "faio_error.h"
#include <sys/prctl.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#define FAIO_WORKER_TO_QUIT                 (1)
#define FAIO_WORKER_NO_QUIT                 (0)
#define FAIO_WORKDE_IDLE_TIMEOUT            (10)
#define FAIO_WORKER_MAX_THREADS             (1024)
#define FAIO_WORKER_MAX_IDLE_TIMEOUT        (600)

static int faio_worker_create_thread(faio_worker_t *tid, 
    void *(*proc)(void *), void *arg,faio_errno_t *err);
static void faio_worker_name_set(void);
static void *faio_worker_fun(void *worker_arg);
static int faio_worker_start_thread(faio_worker_manager_t *worker_mgr, 
	faio_errno_t *err);
static void faio_worker_quit_thread(faio_worker_manager_t *worker_mgr);
static void faio_worker_end_thread(faio_worker_thread_t *self);
static int faio_worker_init_properties(faio_worker_manager_t *worker_mgr, 
	faio_worker_properties_t *properties, faio_errno_t *err);

int faio_worker_set_max_idle(faio_worker_manager_t *worker_mgr, 
    unsigned int max_idle, faio_errno_t *err)
{
    int ret = FAIO_OK;
    
    faio_lock(worker_mgr->work_lock);
	
    if (worker_mgr->worker_properties.max_idle > 
        worker_mgr->worker_properties.max_thread) 
    {
        faio_unlock(worker_mgr->work_lock);
        ret = FAIO_ERROR;
        err->worker = FAIO_WORKER_ERR_SET_MAX_IDLE;
		
        goto quit;
    }
	
    worker_mgr->worker_properties.max_idle = max_idle;
	
    faio_unlock(worker_mgr->work_lock);

quit:
    return ret;
}

int faio_worker_set_max_threads(faio_worker_manager_t *worker_mgr, 
    unsigned int max_threads, faio_errno_t *err)
{   
    int ret = FAIO_OK;
    
    if (max_threads > FAIO_WORKER_MAX_THREADS) 
	{
        ret = FAIO_ERROR;
        err->worker = FAIO_WORKER_ERR_SET_MAX_THREAD;
		
        goto quit;
    }
	
    faio_lock(worker_mgr->work_lock);
	
    worker_mgr->worker_properties.max_thread = max_threads;
	
    faio_unlock(worker_mgr->work_lock);

quit:
    return ret;
}

int faio_worker_set_idle_timeout(faio_worker_manager_t *worker_mgr, 
    unsigned int idle_timeout, faio_errno_t *err)
{
    int ret = FAIO_OK;
    
    if (idle_timeout > FAIO_WORKER_MAX_IDLE_TIMEOUT) 
	{
        ret = FAIO_ERROR;
        err->worker = FAIO_WORKER_ERR_SET_IDLE_TIMEOUT;
		
        goto quit;
    }
	
    faio_lock(worker_mgr->work_lock);
	
    worker_mgr->worker_properties.idle_timeout = idle_timeout;
	
    faio_unlock(worker_mgr->work_lock);

quit:
    return ret;
}

static int faio_worker_init_properties(faio_worker_manager_t *worker_mgr, 
    faio_worker_properties_t *properties, faio_errno_t *err)
{
    int  ret = FAIO_OK;
    long cpu_num;

    cpu_num = sysconf(_SC_NPROCESSORS_CONF);
    if (cpu_num < 0 || cpu_num > FAIO_WORKER_MAX_THREADS) 
	{
        ret = FAIO_ERROR;
        err->worker = FAIO_WORKER_ERR_GET_CPU;
        err->sys = errno;
		
        goto quit;
    }

    if (properties == NULL) 
	{
        worker_mgr->worker_properties.max_thread = (unsigned int)cpu_num * 2;       
        worker_mgr->worker_properties.max_idle = (unsigned int)cpu_num;
        worker_mgr->worker_properties.pre_start = (unsigned int)cpu_num * 2;
        worker_mgr->worker_properties.idle_timeout = FAIO_WORKDE_IDLE_TIMEOUT;
		
        goto quit;
    }
    
    if (properties->max_thread == 0 || 
        properties->max_thread > FAIO_WORKER_MAX_THREADS) 
    {
        ret = FAIO_ERROR;
        err->worker = FAIO_WORKER_ERR_INIT_MAX_THREAD;
		
        goto quit;
    }
    
    if (properties->max_idle == 0 || 
        properties->max_idle > properties->max_thread) 
    {
        ret = FAIO_ERROR;
        err->worker = FAIO_WORKER_ERR_INIT_MAX_IDLE;
		
        goto quit;
    }

    if (properties->pre_start > properties->max_thread) 
	{
        ret = FAIO_ERROR;
        err->worker = FAIO_WORKER_ERR_PRE_START;
		
        goto quit;
    }
    
    if (properties->idle_timeout == 0) 
	{
        ret = FAIO_ERROR;
        err->worker = FAIO_WORKER_ERR_INIT_IDLE_TIMEOUT;
		
        goto quit;
    }

    worker_mgr->worker_properties.max_thread = properties->max_thread;
    worker_mgr->worker_properties.max_idle = properties->max_idle;
    worker_mgr->worker_properties.pre_start = properties->pre_start;
    worker_mgr->worker_properties.idle_timeout = properties->idle_timeout;

quit:
    return ret;
}

static int faio_worker_create_thread(faio_worker_t *tid, 
	void *(*proc)(void *), void *arg, faio_errno_t *err)
{
    int retval = FAIO_OK;
    
    retval = pthread_create(tid, NULL, proc, arg);
    if (retval) 
	{
        err->worker = FAIO_WORKER_ERR_THREAD_CREATE;
        err->sys = retval;
        retval = FAIO_ERROR;
    } 
    
    return retval;
}

int faio_worker_manager_init(faio_manager_t *faio_mgr, 
    faio_worker_properties_t *properties, faio_errno_t *err)
{
    int                     retval = FAIO_OK;
    int                     i;
    faio_worker_manager_t  *manager;
    
    if (!faio_mgr) 
	{
        err->worker = FAIO_WORKER_ERR_MANAGER_NULL;
        retval = FAIO_ERROR;
		
        goto quit;
    }
    
    manager = &(faio_mgr->worker_manager);
    manager->data_mgr = &(faio_mgr->data_manager);
    manager->handler_mgr = &(faio_mgr->handler_manager);
    manager->init_flag = FAIO_FALSE;

    retval = faio_worker_init_properties(manager, properties, err);
    if (retval != FAIO_OK) 
	{
        goto quit;
    }
    
    faio_mutex_init(manager->work_lock);
    faio_cond_init(&manager->worker_wait);
    faio_cond_init(&manager->quit_wait);
	faio_queue_init(&manager->worker_queue);
    manager->started = 0;
    manager->idle = 0;
    manager->want_quit = FAIO_WORKER_NO_QUIT;
    manager->init_flag = FAIO_TRUE;
    
    for (i = 0; i < manager->worker_properties.pre_start; i++) 
	{
        retval = faio_worker_start_thread(manager, err);
        if (retval != FAIO_OK) 
		{
            faio_worker_manager_release(manager, err);
			
            goto quit;
        }
    }

quit:
    return retval;
}


static void faio_worker_name_set(void)
{
    char      name[16 + 1];
    const int namelen = sizeof (name) - 1;
    int       len;

    prctl(PR_GET_NAME, (unsigned long)name, 0, 0, 0);
    name[namelen] = 0;
    len = strlen(name);
    strcpy(name + (len <= namelen - 5 ? len : namelen - 5), "/faio");
    prctl(PR_SET_NAME, (unsigned long)name, 0, 0, 0);

    return;
}

static void *faio_worker_fun(void *worker_arg)
{
    faio_data_task_t               *req;
    struct timespec                 ts;
    faio_worker_thread_t           *self;
    faio_worker_properties_t       *worker_ctl;
    faio_worker_manager_t          *worker_mgr;
    faio_data_manager_t            *data_mgr;
    faio_handler_manager_t         *hanle_mgr;
    faio_notifier_manager_t        *notifier;
    faio_errno_t                    err;
	int								to_quit = FAIO_FALSE;

    self = (faio_worker_thread_t *)worker_arg;
    worker_mgr = self->worker_mgr;
    worker_ctl = &(worker_mgr->worker_properties);
    data_mgr = worker_mgr->data_mgr;
    hanle_mgr = worker_mgr->handler_mgr;
    

    pthread_detach(pthread_self());
    faio_worker_name_set();

    ts.tv_nsec = 0UL;
    ts.tv_sec = 0;
    
    for (;;) 
	{
        for (;;) 
		{
            req = faio_data_pop_req(data_mgr);
            if (!req) 
			{
                break;
            }

            to_quit = FAIO_FALSE;
            if (req->cancel_flag == FAIO_FALSE) 
			{
                req->state = FAIO_STATE_DOING;
                faio_handler_exec(hanle_mgr, req);
                req->state = FAIO_STATE_DONE;
            } 
			else 
			{
                req->state = FAIO_STATE_CANCEL;
            }
                
            notifier = req->notifier;
            req->io_callback(req);
            faio_notifier_send(notifier, &err);
            faio_notifier_count_dec(notifier, &err);
        }

        faio_lock(worker_mgr->work_lock);
		
        if (worker_mgr->want_quit == FAIO_WORKER_TO_QUIT) 
		{
            worker_mgr->started--;
            faio_unlock(worker_mgr->work_lock);
			
            goto quit;
        }
                
        if (to_quit == FAIO_TRUE && 
            (worker_mgr->idle >= worker_ctl->max_idle)) 
        {
            worker_mgr->started--;
            faio_unlock(worker_mgr->work_lock);
			
            goto quit;
        }

        ++worker_mgr->idle;
        if (faio_data_manager_get_size(data_mgr) == 0) 
		{
            if (worker_mgr->idle <= worker_ctl->max_idle) 
			{
                faio_cond_wait(&worker_mgr->worker_wait, 
                    &worker_mgr->work_lock);
            } 
			else 
			{
                ts.tv_sec = time(NULL) + worker_ctl->idle_timeout;
                if (faio_cond_timedwait(&worker_mgr->worker_wait, 
					&worker_mgr->work_lock, &ts) == FAIO_COND_TIMEOUT) 
                {
                    to_quit = FAIO_TRUE; 
                }
            }
        }

        --worker_mgr->idle;
        faio_unlock(worker_mgr->work_lock);
    }

quit:
    faio_worker_end_thread(self);
	
    return NULL;
}

static int faio_worker_start_thread(faio_worker_manager_t *worker_mgr,
    faio_errno_t *err)
{
    faio_worker_thread_t  *wrk;
    int                    ret = FAIO_OK;
    
    wrk = (faio_worker_thread_t *)calloc(1, sizeof(faio_worker_thread_t));
    if (!wrk) 
	{
        ret = FAIO_ERROR;
        err->worker = FAIO_WORKER_ERR_THREAD_NULL;
		
        goto quit;
    }
    
    wrk->worker_mgr = worker_mgr;
    ret = faio_worker_create_thread(&wrk->tid, 
		faio_worker_fun, (void *)wrk, err);
    if (ret == FAIO_ERROR) 
	{
        goto quit;
    }
	
    faio_lock(worker_mgr->work_lock);
    faio_queue_insert_tail(&(worker_mgr->worker_queue), &(wrk->q));
    ++worker_mgr->started;
    faio_unlock(worker_mgr->work_lock);
    
quit: 
    if (ret == FAIO_ERROR) 
	{
        if (wrk) 
		{
            free(wrk);
        }
    }
	
    return ret;
}

int faio_worker_maybe_start_thread(faio_worker_manager_t *worker_mgr, 
    faio_errno_t *err)
{   
    int ret = FAIO_OK;

    if (faio_data_manager_get_size(worker_mgr->data_mgr) == 0) 
	{
        return ret;
    }
	
    faio_lock(worker_mgr->work_lock);
	
    if (faio_data_manager_get_size(worker_mgr->data_mgr) == 0) 
	{
        faio_unlock(worker_mgr->work_lock);
		
        return ret;
    }
	
    if (worker_mgr->idle) 
	{
        faio_cond_signal(&worker_mgr->worker_wait, worker_mgr->started);
    }
	
    if (worker_mgr->started >= worker_mgr->worker_properties.max_thread) 
	{
        faio_unlock(worker_mgr->work_lock);
		
        return ret;
    } 
	else if (worker_mgr->data_mgr->req_queue.size <= worker_mgr->started) 
	{
        faio_unlock(worker_mgr->work_lock);
		
        return ret;
    }
	
    faio_unlock(worker_mgr->work_lock);

    ret = faio_worker_start_thread(worker_mgr, err);

    return ret;
}

static void faio_worker_end_thread(faio_worker_thread_t *self)
{
    faio_worker_manager_t *worker_mgr;

    worker_mgr = self->worker_mgr;
    faio_lock(worker_mgr->work_lock);
	
    if (worker_mgr->want_quit == FAIO_WORKER_TO_QUIT) 
	{
        if (!worker_mgr->started) 
		{
            faio_cond_signal(&worker_mgr->quit_wait, 1);
        }
    }
	
    faio_queue_remove(&(self->q));
	
    faio_unlock(worker_mgr->work_lock);
    
    free(self);
}

static void faio_worker_quit_thread(faio_worker_manager_t *worker_mgr)
{
    int i;
    
    faio_lock(worker_mgr->work_lock);
	
    i = worker_mgr->started;
    worker_mgr->want_quit = FAIO_WORKER_TO_QUIT;
	
    faio_unlock(worker_mgr->work_lock);
    
    for ( ; i > 0; i--) 
	{
        faio_lock(worker_mgr->work_lock);
        faio_cond_signal(&worker_mgr->worker_wait, worker_mgr->started);
        faio_unlock(worker_mgr->work_lock);
    }
    
    return;
}

int faio_worker_manager_release(faio_worker_manager_t *worker_mgr, 
    faio_errno_t *err)
{
    if (worker_mgr->init_flag == FAIO_FALSE) 
	{
        err->worker = FAIO_WORKER_ERR_NO_INIT;
		
        return FAIO_ERROR;
    }
    
    faio_worker_quit_thread(worker_mgr);
	
    faio_lock(worker_mgr->work_lock);
	
    while (!faio_queue_empty(&(worker_mgr->worker_queue))) 
	{
        faio_cond_wait(&worker_mgr->quit_wait, &worker_mgr->work_lock);
    }
	
    faio_unlock(worker_mgr->work_lock);
    
    faio_mutex_destroy(worker_mgr->work_lock);
    faio_cond_destroy(worker_mgr->worker_wait);
    faio_cond_destroy(worker_mgr->quit_wait);
    
    return FAIO_OK;
}

