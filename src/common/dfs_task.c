#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "dfs_task.h"

task_t * task_new()
{
	task_t* t = (task_t *)malloc(sizeof(task_t));
	if (!t) 
	{
		return NULL;
	}
	
	memset(t, 0x00, sizeof(task_t));
	
	return t;
}

void task_free(task_t* task)
{
	assert(task);
	
	free(task);
}

void task_clear(task_t* task)
{
	assert(task);
	
	memset(task, 0x00, sizeof(task_t));
}

int task_encode2str(task_t *task, char *buff, int len)
{
	int need_size = 0;
    int pkg_size = (int)sizeof(int);
    int task_size = sizeof(task_t);
    int data_size = (int)sizeof(int);

    need_size = pkg_size + task_size + data_size + task->data_len;
    if (len < need_size)
    {
        return TASK_EAGIN;
    }

    *(int *)buff = need_size;
    buff += pkg_size;

    memcpy(buff, task, task_size);
    buff += task_size;

    *(int *)buff = task->data_len;
    buff += data_size;

    if (task->data_len > 0) 
    {
        memcpy(buff, task->data, task->data_len);
    }

    return need_size;
}

int task_decodefstr(char *buff, int len, task_t *task)
{
    void *data = task->opq;
    int need_size = *(int *)buff;
    int pkg_size = (int)sizeof(int);
    int task_size = sizeof(task_t);
    int data_size = (int)sizeof(int);

    if (len < need_size)
    {
        return TASK_EAGIN;
    }

    buff += pkg_size;

    memcpy(task, buff, task_size);
    buff += task_size;

    int data_len = *(int *)buff;
    buff += data_size;

    if (data_len > 0)
    {
        task->data = buff;
    }

	task->opq = data;

    return need_size;
}

