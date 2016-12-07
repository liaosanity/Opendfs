#include <pthread.h>
#include <errno.h>
#include "faio_thread.h"
#include "faio_error.h"
#include "faio_manager.h"
#include "faio_data_manager.h"
#include "faio_handler_manager.h"
#include "faio_worker_manager.h"
#include "faio_notifier_manager.h"

int faio_manager_init(faio_manager_t *faio_mgr, faio_properties_t *faio_prop,
    unsigned int max_task_num, faio_errno_t *error)
{
    faio_handler_manager_t *handler_mgr = NULL;

    if (!error) 
	{
        return FAIO_ERROR;
    }
    
    if (!faio_mgr) 
	{
        error->data = FAIO_ERR_DATA_MANAGER_NULL;
		
        return FAIO_ERROR;
    }

    memset(faio_mgr, 0x00, sizeof(*faio_mgr));

    faio_mgr->release_flag = FAIO_FALSE;

    if (faio_data_manager_init(faio_mgr, max_task_num, error) == FAIO_ERROR) 
	{
        goto release;
    }

    if (faio_worker_manager_init(faio_mgr, faio_prop, error) == FAIO_ERROR) 
	{
        goto release;
    }

    handler_mgr = &faio_mgr->handler_manager;

    faio_handler_manager_init(handler_mgr);

    return FAIO_OK;

release:
    faio_manager_release(faio_mgr, error);
	
    return FAIO_ERROR;
}

int faio_manager_release(faio_manager_t *faio_mgr, faio_errno_t *error)
{
    faio_data_manager_t *data_mgr = NULL;
    
    if (!error) 
	{
        return FAIO_ERROR;
    }
    
    if (!faio_mgr) 
	{
        error->data = FAIO_ERR_DATA_MANAGER_NULL;
		
        return FAIO_ERROR;
    }

    data_mgr = &faio_mgr->data_manager;
    
    faio_mgr->release_flag = FAIO_TRUE;

    if (faio_worker_manager_release(&faio_mgr->worker_manager, error) 
        == FAIO_ERROR) 
    {
        return FAIO_ERROR;
    }
    
    if (faio_data_manager_release(&faio_mgr->data_manager, error) 
        == FAIO_ERROR) 
    {
        return FAIO_ERROR;
    }

    faio_handler_manager_release(&faio_mgr->handler_manager);

    return FAIO_OK;
}

int faio_notifier_init(faio_notifier_manager_t *notifier_mgr, 
    faio_manager_t *faio_mgr, faio_errno_t *error)
{
    if (!error) 
	{
        return FAIO_ERROR;
    }
    
    if (!notifier_mgr) 
	{
        error->data = FAIO_ERR_DATA_NOTIFIER_NULL;
		
        return FAIO_ERROR;
    }

    if (!faio_mgr) 
	{
        error->data = FAIO_ERR_DATA_MANAGER_NULL;
		
        return FAIO_ERROR;
    }

    return faio_notifier_manager_init(notifier_mgr, faio_mgr, error);
}

int faio_notifier_release(faio_notifier_manager_t *notifier_mgr, 
    faio_errno_t *error)
{
    if (!error) 
	{
        return FAIO_ERROR;
    }
    
    if (!notifier_mgr) 
	{
        error->data = FAIO_ERR_DATA_NOTIFIER_NULL;
		
        return FAIO_ERROR;
    }

    return faio_notifier_manager_release(notifier_mgr, error);
}

int faio_register_handler(faio_manager_t *faio_mgr, 
	faio_io_handler_t handler, FAIO_IO_TYPE io_type, faio_errno_t *error)
{
    if (!error) 
	{
        return FAIO_ERROR;
    }
    
    if (!faio_mgr) 
	{
        error->data = FAIO_ERR_DATA_MANAGER_NULL;
		
        return FAIO_ERROR;
    }

    if (io_type >= FAIO_IO_TYPE_READ && io_type < FAIO_IO_TYPE_END) 
	{
        return faio_handler_register(&faio_mgr->handler_manager, 
            handler, io_type, error);
    } 
	
    error->data = FAIO_ERR_DATA_IOTYPE_WRONG;
	
    return FAIO_ERROR;
}

int faio_read(faio_notifier_manager_t *notifier_mgr, 
	faio_callback_t faio_callback, faio_data_task_t *task, faio_errno_t *error)
{
    faio_manager_t        *faio_mgr = NULL;
    faio_data_manager_t   *data_mgr = NULL;
    faio_worker_manager_t *worker_mgr = NULL;

    if (!error) 
	{
        return FAIO_ERROR;
    }
    
    if (!notifier_mgr) 
	{
        error->data = FAIO_ERR_DATA_NOTIFIER_NULL;
		
        return FAIO_ERROR;
    }

    faio_mgr = notifier_mgr->manager;
    data_mgr = &faio_mgr->data_manager;
    worker_mgr = &faio_mgr->worker_manager;
    
    if (faio_data_push_task(data_mgr, task, notifier_mgr, faio_callback, 
        FAIO_IO_TYPE_READ, error) == FAIO_ERROR) 
    {
        return FAIO_ERROR;
    }

    faio_notifier_count_inc(notifier_mgr, error);
    faio_worker_maybe_start_thread(worker_mgr, error);

    return FAIO_OK;
}

int faio_write(faio_notifier_manager_t *notifier_mgr, 
	faio_callback_t faio_callback, faio_data_task_t *task, faio_errno_t *error)
{
    faio_manager_t        *faio_mgr = NULL;
    faio_data_manager_t   *data_mgr = NULL;
    faio_worker_manager_t *worker_mgr = NULL;

    if (!error) 
	{
        return FAIO_ERROR;
    }
    
    if (!notifier_mgr) 
	{
        error->data = FAIO_ERR_DATA_NOTIFIER_NULL;
		
        return FAIO_ERROR;
    }

    faio_mgr = notifier_mgr->manager;
    data_mgr = &faio_mgr->data_manager;
    worker_mgr = &faio_mgr->worker_manager;
    
    if (faio_data_push_task(data_mgr, task, notifier_mgr, faio_callback, 
        FAIO_IO_TYPE_WRITE, error) == FAIO_ERROR) 
    {
        return FAIO_ERROR;
    }

    faio_notifier_count_inc(notifier_mgr, error);
    faio_worker_maybe_start_thread(worker_mgr, error);

    return FAIO_OK;
}

int faio_sendfile(faio_notifier_manager_t *notifier_mgr, 
	faio_callback_t faio_callback, faio_data_task_t *task, faio_errno_t *error)
{
    faio_manager_t        *faio_mgr = NULL;
    faio_data_manager_t   *data_mgr = NULL;
    faio_worker_manager_t *worker_mgr = NULL;

    if (!error) 
	{
        return FAIO_ERROR;
    }
    
    if (!notifier_mgr) 
	{
        error->data = FAIO_ERR_DATA_SENDFILE_NOTIFIER_NULL;
		
        return FAIO_ERROR;
    }

    faio_mgr = notifier_mgr->manager;
    data_mgr = &faio_mgr->data_manager;
    worker_mgr = &faio_mgr->worker_manager;
    
    if (faio_data_push_task(data_mgr, task, notifier_mgr, faio_callback, 
        FAIO_IO_TYPE_SENDFILE, error) == FAIO_ERROR) 
    {
        return FAIO_ERROR;
    }

    faio_notifier_count_inc(notifier_mgr, error);
    faio_worker_maybe_start_thread(worker_mgr, error);

    return FAIO_OK;
}

int faio_recv_notifier(faio_notifier_manager_t *notifier_mgr, 
	faio_errno_t *error)
{
    int ret = FAIO_ERROR;

    if (!error) 
	{
        return FAIO_ERROR;
    }
    
    if (!notifier_mgr) 
	{
        error->data = FAIO_ERR_DATA_NOTIFIER_NULL;
		
        return FAIO_ERROR;
    }

    ret = faio_notifier_receive(notifier_mgr, error);

    return ret;
}

int faio_remove_task(faio_data_task_t *task, faio_errno_t *error)
{
    if (!error) 
	{
        return FAIO_ERROR;
    }
    
    if (!task) 
	{
        error->data = FAIO_ERR_DATA_TASK_NULL;
		
        return FAIO_ERROR;
    }

    task->cancel_flag = FAIO_TRUE;

    return FAIO_OK;
}

int faio_set_max_idle(faio_manager_t *faio_mgr, unsigned int max_idle, 
	faio_errno_t *error)
{
    faio_worker_manager_t *worker_mgr = NULL;

    if (!error) 
	{
        return FAIO_ERROR;
    }
    
    if (!faio_mgr) 
	{
        error->data = FAIO_ERR_DATA_MANAGER_NULL;
		
        return FAIO_ERROR;
    }

    worker_mgr = &faio_mgr->worker_manager;
    
    if (faio_worker_set_max_idle(worker_mgr, max_idle, error) != FAIO_OK) 
	{
        return FAIO_ERROR;
    }

    return FAIO_OK;
}

int faio_set_max_threads(faio_manager_t *faio_mgr, 
	unsigned int max_threads, faio_errno_t *error)
{
    faio_worker_manager_t *worker_mgr = NULL;

    if (!error) 
	{
        return FAIO_ERROR;
    }
    
    if (!faio_mgr) 
	{
        error->data = FAIO_ERR_DATA_MANAGER_NULL;
		
        return FAIO_ERROR;
    }

    worker_mgr = &faio_mgr->worker_manager;
    
    if (faio_worker_set_max_threads(worker_mgr, max_threads, error) 
        != FAIO_OK) 
    {
        return FAIO_ERROR;
    }

    return FAIO_OK;
}

int faio_set_idle_timeout(faio_manager_t *faio_mgr, 
	unsigned int idle_timeout, faio_errno_t *error)
{
    faio_worker_manager_t *worker_mgr = NULL;

    if (!error) 
	{
        return FAIO_ERROR;
    }
    
    if (!faio_mgr) 
	{
        error->data = FAIO_ERR_DATA_MANAGER_NULL;
		
        return FAIO_ERROR;
    }

    worker_mgr = &faio_mgr->worker_manager;
    
    if (faio_worker_set_idle_timeout(worker_mgr, idle_timeout, error) 
        != FAIO_OK) 
    {
        return FAIO_ERROR;
    }

    return FAIO_OK;
}

int faio_get_task_errno(faio_data_task_t *task, int *err, int *sys, 
    faio_errno_t *error)
{
    if (!task || !err || !sys) 
	{
        error->data = FAIO_ERR_DATA_PARAM_NULL;
		
        return FAIO_ERROR;
    }

    *err = task->err.err;
    *sys = task->err.sys;

    return FAIO_OK;
}

const char *faio_get_error_msg(faio_errno_t *err_no)
{
    return faio_error_msg(err_no);
}

