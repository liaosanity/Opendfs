#ifndef DFS_COMM_POOL_H
#define DFS_COMM_POOL_H

#include <stdint.h>
#include <stdlib.h>
#include <assert.h>

#include "dfs_types.h"
#include "dfs_mem_allocator.h"

typedef struct mpool_mgmt_param_s 
{
    void   *mem_addr;
    size_t  mem_size;
} mpool_mgmt_param_t;

typedef struct mpool_mgmt_s
{
    void   *start;
    void   *free;
    size_t  mem_size;
} mpool_mgmt_t;

mpool_mgmt_t * mpool_mgmt_create(mpool_mgmt_param_t *param);
void * mpool_alloc(mpool_mgmt_t *pm, size_t size);
void destroy_mpool_mgmt(mpool_mgmt_t *pm);

#endif

