#include "dfs_task_codec.h"

int task_decode(buffer_t *buff, task_t *task)
{
    int ret = 0;

    if (buffer_size(buff) <= 0) 
	{
		return DFS_AGAIN;
    }

    ret = task_decodefstr((char*)buff->pos, buffer_size(buff), task);
    if (ret <= 0) 
	{
        return ret;
    }
        
    buff->pos +=ret;

    return DFS_OK;
}

int task_encode(task_t *task, buffer_t *buff)
{
	int ret = 0;

	if (buffer_free_size(buff) <= 0) 
	{
		return DFS_AGAIN;
	}
		
	ret = task_encode2str(task, (char*)buff->last, buffer_free_size(buff));
	if (ret <= 0) 
	{
	    return ret;
	}
	        
	buff->last += ret;

	return DFS_OK;
}

