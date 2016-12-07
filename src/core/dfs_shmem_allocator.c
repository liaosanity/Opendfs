#include "dfs_shmem_allocator.h"
#include "dfs_shmem.h"
#include "dfs_memory.h"

static int dfs_shmem_allocator_init(dfs_mem_allocator_t *me, 
    void *param_data);
static int dfs_shmem_allocator_release(dfs_mem_allocator_t *me, 
	void *param_data);
static void * dfs_shmem_allocator_alloc(dfs_mem_allocator_t *me, 
	size_t size, void *param_data);
static void * dfs_shmem_allocator_calloc(dfs_mem_allocator_t *me, 
	size_t size, void *param_data);
static void * dfs_shmem_allocator_split_alloc(dfs_mem_allocator_t *me, 
	size_t *act_size, size_t req_minsize, void *param_data);
static int dfs_shmem_allocator_free(dfs_mem_allocator_t *me, 
	void *ptr, void *param_data);
static const char * dfs_shmem_allocator_strerror(dfs_mem_allocator_t *me, 
	void *err_data);
static int dfs_shmem_allocator_stat(dfs_mem_allocator_t *me, 
	void *stat_data);

static dfs_mem_allocator_t dfs_shmem_allocator; 
//static dfs_mem_allocator_t dfs_shmem_allocator = 
//{
//    .private_data   = NULL,
//    .type           = DFS_MEM_ALLOCATOR_TYPE_SHMEM,
//    .init           = dfs_shmem_allocator_init,
//    .release        = dfs_shmem_allocator_release,
//    .alloc          = dfs_shmem_allocator_alloc,
//    .calloc         = dfs_shmem_allocator_calloc,
//    .split_alloc    = dfs_shmem_allocator_split_alloc,
//    .free           = dfs_shmem_allocator_free,
//    .strerror       = dfs_shmem_allocator_strerror,
//    .stat           = dfs_shmem_allocator_stat
//};

static int dfs_shmem_allocator_init(dfs_mem_allocator_t *me, 
	                                        void *param_data)
{
    dfs_shmem_allocator_param_t *param =
         (dfs_shmem_allocator_param_t *)param_data;
         
    if (!me || !param) 
	{
        return DFS_MEM_ALLOCATOR_ERROR; 
    }
	
    if (me->private_data) 
	{
        return DFS_MEM_ALLOCATOR_OK;
    }
    
    me->private_data = dfs_shmem_create(param->size, param->min_size,
         param->max_size, param->level_type, param->factor, &param->err_no); 
    if (!me->private_data) 
	{
        return DFS_MEM_ALLOCATOR_ERROR;
    }
	
    return DFS_MEM_ALLOCATOR_OK;
}

static int dfs_shmem_allocator_release(dfs_mem_allocator_t *me, 
	                                              void *param_data)
{
    dfs_shmem_allocator_param_t  *param = NULL;
    dfs_shmem_t                 **shmem = NULL;

    param = (dfs_shmem_allocator_param_t *)param_data;

    if (!me || !me->private_data) 
	{
        return DFS_MEM_ALLOCATOR_ERROR;
    }
    
    shmem = (dfs_shmem_t **)&me->private_data;
    if (dfs_shmem_release(shmem, &param->err_no) == DFS_SHMEM_ERROR) 
	{
        return DFS_MEM_ALLOCATOR_ERROR;
    }
	
    return DFS_MEM_ALLOCATOR_OK;
}

static void * dfs_shmem_allocator_alloc(dfs_mem_allocator_t *me, 
	                                             size_t size, void *param_data)
{
    unsigned int  shmem_errno = -1;
    dfs_shmem_t  *shmem = NULL;
    void         *ptr = NULL;

    if (!me || !me->private_data) 
	{
        return NULL;
    }
	
    shmem = (dfs_shmem_t *)me->private_data;
    ptr = dfs_shmem_alloc(shmem, size, &shmem_errno);
    if (!ptr) 
	{
        if (param_data) 
		{
            *(unsigned int *)param_data = shmem_errno;
        }
    }
	
    return ptr;
}

static void * dfs_shmem_allocator_calloc(dfs_mem_allocator_t *me, 
                                                   size_t size, 
                                                   void *param_data)
{
    void *ptr = NULL;
	
    if (!me) 
	{
        return NULL;
    }
	
    ptr = me->alloc(me, size, param_data);
    if (ptr) 
	{
        memory_zero(ptr, size);
    }
	
    return ptr;
}

static void * dfs_shmem_allocator_split_alloc(dfs_mem_allocator_t *me, 
	                                                     size_t *act_size, 
	                                                     size_t req_minsize, 
	                                                     void *param_data)
{
    dfs_shmem_t  *shmem = NULL;
    unsigned int *shmem_errno = NULL;

    if (!me || !me->private_data || !param_data) 
	{
        return NULL;
    }
	
    shmem = (dfs_shmem_t *)me->private_data;
    shmem_errno = (unsigned int *)param_data;

    return dfs_shmem_split_alloc(shmem, act_size, req_minsize, shmem_errno);
}

static int dfs_shmem_allocator_free(dfs_mem_allocator_t *me, 
	                                         void *ptr, void *param_data)
{
    unsigned int  shmem_errno = -1;
    dfs_shmem_t  *shmem = NULL;  

    if (!me || !me->private_data || !ptr) 
	{
        return DFS_MEM_ALLOCATOR_ERROR;
    }

    shmem = (dfs_shmem_t *)me->private_data;
    dfs_shmem_free(shmem, ptr, &shmem_errno);

    if (shmem_errno != DFS_SHMEM_ERR_NONE) 
	{
        if (param_data) 
		{
            *(unsigned int *)param_data = shmem_errno;
        }
		
        return DFS_MEM_ALLOCATOR_ERROR;
    }
	
    return DFS_MEM_ALLOCATOR_OK;
}

static const char * dfs_shmem_allocator_strerror(dfs_mem_allocator_t *me, 
	                                                       void *err_data)
{
    if (!me || !err_data) 
	{
        return NULL;
    }
	
    return dfs_shmem_strerror(*(unsigned int *)err_data); 
}

static int dfs_shmem_allocator_stat(dfs_mem_allocator_t *me, 
	                                         void *stat_data)
{
    dfs_shmem_stat_t *stat = (dfs_shmem_stat_t *)stat_data;
    dfs_shmem_t      *shmem = NULL;

    if (!me || !me->private_data || !stat) 
	{
        return DFS_MEM_ALLOCATOR_ERROR;
    }
   
    shmem = (dfs_shmem_t *)me->private_data;
    if (dfs_shmem_get_stat(shmem, stat) == DFS_SHMEM_ERROR) 
	{
        return DFS_MEM_ALLOCATOR_ERROR;
    }
	
    return DFS_MEM_ALLOCATOR_OK;
}

const dfs_mem_allocator_t * dfs_get_shmem_allocator(void)
{
    dfs_shmem_allocator.private_data   = NULL;
    dfs_shmem_allocator.type           = DFS_MEM_ALLOCATOR_TYPE_SHMEM;
    dfs_shmem_allocator.init           = dfs_shmem_allocator_init;
    dfs_shmem_allocator.release        = dfs_shmem_allocator_release;
    dfs_shmem_allocator.alloc          = dfs_shmem_allocator_alloc;
    dfs_shmem_allocator.calloc         = dfs_shmem_allocator_calloc;
    dfs_shmem_allocator.split_alloc    = dfs_shmem_allocator_split_alloc;
    dfs_shmem_allocator.free           = dfs_shmem_allocator_free;
    dfs_shmem_allocator.strerror       = dfs_shmem_allocator_strerror;
    dfs_shmem_allocator.stat           = dfs_shmem_allocator_stat;

    return &dfs_shmem_allocator;
}

