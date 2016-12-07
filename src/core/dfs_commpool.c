#include "dfs_commpool.h"

#define DFS_ALIGNMENT sizeof(uint64_t)
#define dfs_align_ptr(p, a)       \
        (uchar_t *) (((uintptr_t)(p) + ((uintptr_t)(a) - 1)) & ~((uintptr_t)(a) - 1))

mpool_mgmt_t * mpool_mgmt_create(mpool_mgmt_param_t *param)
{
    mpool_mgmt_t *pm = NULL;

    pm = (mpool_mgmt_t *)param->mem_addr;
    pm->start = param->mem_addr;
    pm->free = (void*) pm + sizeof(mpool_mgmt_t);
    pm->mem_size = param->mem_size;

    return pm;
}

void * mpool_alloc(mpool_mgmt_t *pm, size_t size)
{
    assert(pm);

    void *ptr = NULL;

    pm->free = dfs_align_ptr(pm->free, DFS_ALIGNMENT);
    if (pm->free > pm->start + pm->mem_size) 
	{
        pm->free = pm->start + pm->mem_size;
		
        return NULL;
    }

    if (size + pm->free > pm->start + pm->mem_size) 
	{
        return NULL;
    }
    
    ptr = pm->free;
    pm->free += size;

    return ptr;
}

void destroy_mpool_mgmt(mpool_mgmt_t *pm)
{
    assert(pm);
	
    (void)pm;
	
    return;
}

