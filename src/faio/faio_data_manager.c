#include "faio_manager.h"
#include "faio_error.h"

int faio_data_manager_init(faio_manager_t *faio_mgr, 
    unsigned int max_task, faio_errno_t *error)
{   
    faio_data_manager_t *data_mgr = NULL;

    data_mgr = &faio_mgr->data_manager;
    data_mgr->faio_mgr = faio_mgr;
    data_mgr->req_queue.queue_start = NULL;
    data_mgr->req_queue.queue_end = NULL;
    data_mgr->req_queue.size = 0;

    if (max_task == 0) 
	{
        data_mgr->max_size = DEFAULT_QUE_SIZE;
    } 
	else 
	{
        data_mgr->max_size = max_task;
    }

    faio_atomic_lock_init(data_mgr->req_queue.lock);

    return FAIO_OK;
}

size_t faio_data_manager_get_size(faio_data_manager_t *data_mgr)
{
    return data_mgr->req_queue.size;
}

int faio_data_manager_release(faio_data_manager_t *data_mgr, 
    faio_errno_t *error)
{    
    data_mgr->faio_mgr = NULL;
    data_mgr->req_queue.queue_start = NULL;
    data_mgr->req_queue.queue_end = NULL;
    data_mgr->req_queue.size = 0;
    data_mgr->max_size = 0;

    return FAIO_OK;
}

static void faio_data_push_req(faio_data_queue_t *que, 
	faio_data_task_t *req_task)
{
    faio_atomic_lock(&que->lock);
	
    if (que->size != 0) 
	{
        que->queue_end->next = req_task;
        que->queue_end = req_task;
    } 
	else 
	{
        que->queue_start = req_task;
        que->queue_end = req_task;
    }
	
    que->size++;
	
    faio_atomic_unlock(&que->lock);
}

faio_data_task_t *faio_data_pop_req(faio_data_manager_t *data_mgr)
{
    faio_data_task_t  *req_task = NULL;
    faio_data_queue_t *que = NULL;

    que = &data_mgr->req_queue;

    faio_atomic_lock(&(que->lock));
	
    if (que->size != 0) 
	{
        req_task = que->queue_start;
        if (req_task) 
		{
            if (!(que->queue_start = req_task->next)) 
			{
                que->queue_end = NULL;
            }
        } 
		else 
		{
            faio_atomic_unlock(&(que->lock));
			
            return NULL;
        }
        
        req_task->next = NULL;
        que->size--;
    }
	
    faio_atomic_unlock(&(que->lock));

    return req_task;
}

int faio_data_push_task(faio_data_manager_t *data_mgr, 
	faio_data_task_t *task, faio_notifier_manager_t *notifier, 
	faio_callback_t io_callback, FAIO_IO_TYPE io_type, faio_errno_t *error)
{
    faio_data_queue_t *que = NULL;
    
    if (!io_callback) 
	{
        error->data = FAIO_ERR_DATA_CALLBACK_NULL;
		
        return FAIO_ERROR;
    }

    if (!task) 
	{
        error->data = FAIO_ERR_DATA_TASK_NULL;
		
        return FAIO_ERROR;
    }

    task->state = FAIO_STATE_WAIT;
    task->cancel_flag = FAIO_FALSE;
    task->next = NULL;
    task->io_type = io_type;
    task->io_callback = io_callback;
    task->notifier = notifier;
    task->err.err = FAIO_ERR_TASK_NO_ERR; 
    task->err.sys =FAIO_ERR_TASK_NO_ERR;

    que = &data_mgr->req_queue;

    if (que->size >= data_mgr->max_size) 
	{
        error->data = FAIO_ERR_DATA_TASK_TOO_MANY;
        task->err.err = FAIO_ERR_TASK_TOO_MANY;
		
        return FAIO_ERROR;
    }
    
    if (data_mgr->faio_mgr->release_flag == FAIO_TRUE) 
	{
        return FAIO_ERROR;
    }
    
    faio_data_push_req(que, task);
    
    return FAIO_OK;
}
