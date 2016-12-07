#include "faio_error.h"

const char *faio_error_msg(faio_errno_t *err_no)
{
    int ret = -1;
    
    if (!err_no) 
	{
        return NULL;
    }
    
    ret = snprintf(err_no->msg, FAIO_ERR_MSG_MAX_LEN,
        "faio error number: data=%d, worker=%d, handler=%d, notifier=%d, sys=%d",
        err_no->data, err_no->worker, err_no->handler, err_no->notifier, 
        err_no->sys);
    err_no->msg[ret] = 0;
    
    return err_no->msg;
}

