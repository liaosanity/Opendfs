#include "faio_error.h"
#include "faio_thread.h"
#include "faio_manager.h"
#include "faio_handler_manager.h"

void faio_handler_manager_init(faio_handler_manager_t *handler_mgr)
{
    int i = 0;

    for (i = FAIO_IO_TYPE_READ; i < FAIO_IO_TYPE_END; i++) 
	{
        handler_mgr->handler[i] = NULL;
    }
}

void faio_handler_manager_release(faio_handler_manager_t *handler_mgr)
{
    int i = 0;

    for (i = FAIO_IO_TYPE_READ; i < FAIO_IO_TYPE_END; i++) 
	{
        handler_mgr->handler[i] = NULL;
    }
}

int faio_handler_register(faio_handler_manager_t *handler_mgr, 
    faio_io_handler_t handler, FAIO_IO_TYPE io_type, faio_errno_t *error)
{
    if (!handler) 
	{
        error->handler = FAIO_ERR_HANDLER_NULL;
		
        return FAIO_ERROR;
    }

    if (!handler_mgr->handler[io_type]) 
	{
        handler_mgr->handler[io_type] = handler;
		
        return FAIO_OK;     
    } 
	
    error->handler = FAIO_ERR_HANDLER_REDEF;
	
    return FAIO_ERROR;
}

void faio_handler_exec(faio_handler_manager_t *handler_mgr, 
    faio_data_task_t *task)
{
    int io_type = 0;

    io_type = task->io_type;
    
    if (io_type < FAIO_IO_TYPE_READ || io_type >= FAIO_IO_TYPE_END) 
	{
        task->err.err= FAIO_ERR_TASK_IOTYPE_WRONG;
		
        return;
    }

    if (!handler_mgr->handler[io_type]) 
	{
        task->err.err = FAIO_ERR_TASK_HANDLER_NULL;
		
        return;
    }

    if (handler_mgr->handler[io_type](task) != FAIO_OK) 
	{
		task->err.err = FAIO_ERR_TASK_HANDLER_ERR;
		
		return;
   	}

	return;
}

