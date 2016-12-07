#ifndef DFS_MEMPOOL_ALLOCATOR_H
#define DFS_MEMPOOL_ALLOCATOR_H

#include "dfs_mem_allocator.h"

typedef struct dfs_mempool_allocator_param_s 
{
    size_t pool_size;
    size_t max_size;
} dfs_mempool_allocator_param_t;

#endif

