#ifndef DFS_PIPE_H
#define DFS_PIPE_H

#include "dfs_types.h"

#define PIPE_MAX_SIZE 8192

typedef struct pipe_s 
{
    int    pfd[2];
    size_t size;
} pipe_t;

int  pipe_open(pipe_t *p);
void pipe_close(pipe_t *p);

#endif

