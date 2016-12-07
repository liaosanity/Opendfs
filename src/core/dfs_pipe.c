#include "dfs_pipe.h"

int pipe_open(pipe_t *p)
{
    if (!p) 
	{
        return DFS_ERROR;
    }

    errno = 0;
    if (pipe(p->pfd)) 
	{
        return DFS_ERROR;
    }

    return DFS_OK;
}

void pipe_close(pipe_t *p)
{
    if (!p) 
	{
        return;
    }

    if (p->pfd[0] >= 0) 
	{
        close(p->pfd[0]);
    }

    if (p->pfd[1] >= 0) 
	{
        close(p->pfd[1]);
    }

    p->pfd[0] = DFS_INVALID_FILE;
    p->pfd[1] = DFS_INVALID_FILE;
    p->size = 0;
}

