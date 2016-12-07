#include "dfs_mempool_allocator.h"
#include "dfs_memory_pool.h"
#include "dfs_memory.h"

static int dfs_mempool_allocator_init(dfs_mem_allocator_t *me, 
    void *param_data);
static int dfs_mempool_allocator_release(dfs_mem_allocator_t *me, 
    void *param_data);
static void * dfs_mempool_allocator_alloc(dfs_mem_allocator_t *me, 
	size_t size, void *param_data);
static void *dfs_mempool_allocator_calloc(dfs_mem_allocator_t *me,
	size_t size, void *param_data);
static const char * dfs_mempool_allocator_strerror(dfs_mem_allocator_t *me, 
	void *err_data);

static dfs_mem_allocator_t dfs_mempool_allocator;  
//static dfs_mem_allocator_t dfs_mempool_allocator = 
//{
//    .private_data = NULL,
//    .type         = DFS_MEM_ALLOCATOR_TYPE_MEMPOOL,
//    .init         = dfs_mempool_allocator_init,
//    .release      = dfs_mempool_allocator_release,
//    .alloc        = dfs_mempool_allocator_alloc,
//    .calloc       = dfs_mempool_allocator_calloc,
//    .free         = NULL,
//    .strerror     = dfs_mempool_allocator_strerror,
//    .stat         = NULL
//};

static int dfs_mempool_allocator_init(dfs_mem_allocator_t *me, 
	                                           void *param_data)
{
    dfs_mempool_allocator_param_t *param = NULL;

    if (!me || !param_data) 
	{
        return DFS_MEM_ALLOCATOR_ERROR;
    }
	
    if (me->private_data) 
	{
        return DFS_MEM_ALLOCATOR_OK;
    }
	
    param = (dfs_mempool_allocator_param_t *)param_data;
    me->private_data = pool_create(param->pool_size, 
		param->max_size, me->log);
    if (!me->private_data) 
	{
        return DFS_MEM_ALLOCATOR_ERROR;
    }
	
    return DFS_MEM_ALLOCATOR_OK;
}

static int dfs_mempool_allocator_release(dfs_mem_allocator_t *me, 
	                                                void *param_data)
{
    pool_t *pool = NULL;

    if (!me || !me->private_data) 
	{
        return DFS_MEM_ALLOCATOR_ERROR;
    }
	
    pool = (pool_t *)me->private_data;
    pool_destroy(pool);
    me->private_data = NULL;

    return DFS_MEM_ALLOCATOR_OK;
}

static void * dfs_mempool_allocator_alloc(dfs_mem_allocator_t *me, 
	                                                size_t size, 
	                                                void *param_data)
{
    pool_t *pool = NULL;

    if (!me || !me->private_data) 
	{
        return NULL;
    }

    pool = (pool_t *)me->private_data;

    return pool_alloc(pool, size);
}

static void * dfs_mempool_allocator_calloc(dfs_mem_allocator_t *me, 
	                                                  size_t size, 
	                                                  void *param_data)
{
    void *ptr = NULL;
	
    ptr = me->alloc(me, size, param_data); 
    if (ptr) 
	{
        memory_zero(ptr, size);
    }
	
    return ptr;
}

static const char * dfs_mempool_allocator_strerror(dfs_mem_allocator_t *me, 
	                                                          void *err_data)
{
    return "no more error info";
}

const dfs_mem_allocator_t * dfs_get_mempool_allocator(void)
{
    dfs_mempool_allocator.private_data = NULL;
    dfs_mempool_allocator.type         = DFS_MEM_ALLOCATOR_TYPE_MEMPOOL;
    dfs_mempool_allocator.init         = dfs_mempool_allocator_init;
    dfs_mempool_allocator.release      = dfs_mempool_allocator_release;
    dfs_mempool_allocator.alloc        = dfs_mempool_allocator_alloc;
    dfs_mempool_allocator.calloc       = dfs_mempool_allocator_calloc;
    dfs_mempool_allocator.free         = NULL;
    dfs_mempool_allocator.strerror     = dfs_mempool_allocator_strerror;
    dfs_mempool_allocator.stat         = NULL;

    return &dfs_mempool_allocator;
}

