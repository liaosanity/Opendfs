#ifndef DFS_SHMEM_ALLOCATOR_H
#define DFS_SHMEM_ALLOCATOR_H

#include "dfs_mem_allocator.h"

typedef struct dfs_shmem_allocator_param_s 
{
    size_t       size;
    size_t       min_size;
    size_t       max_size;
    size_t       factor;
    int          level_type;
    unsigned int err_no;
} dfs_shmem_allocator_param_t;

#endif

