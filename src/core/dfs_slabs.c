#include "dfs_slabs.h"
#include "dfs_math.h"

#define DFS_SLAB_ERRNO_CLEAN(err)    \
    (err)->slab_errno = DFS_SLAB_ERR_NONE;   \
    (err)->allocator_errno = DFS_SLAB_ERR_NONE

static const char* dfs_slabs_err_string[] = 
{
    "dfs_slabs: unknown error number",
    "dfs_slabs_create: parameter error",
    "dfs_slabs_create: power factor error",
    "dfs_slabs_create: liner factor error",
    "dfs_slabs_create: uptype error",
    "dfs_slabs_release: parameter error",
    "dfs_slabs_alloc: parameter error",
    "dfs_slabs_alloc: invalide id",
    "dfs_slabs_alloc: alloc failed",
    "dfs_slabs_split_alloc: parameter error",
    "dfs_slabs_split_alloc: not supported or implemented",
    "dfs_slabs_split_alloc: chunk size too large",
    "dfs_slabs_free: parameter error",
    "dfs_slabs_free: not supported or implemented",
    "dfs_slabs_free: chunk id error",
    "dfs_slabs: allocator error",
    NULL
};

static ssize_t dfs_slabs_clsid(dfs_slab_manager_t *slab_mgr, 
	                                size_t size, int up)
{
    ssize_t id = -1;
    ssize_t mid = -1;
    ssize_t high = -1;
    ssize_t low = 1;
    ssize_t max_id = -1;
    size_t  all_size = -1;

    if (!size || !slab_mgr) 
	{
        return DFS_SLAB_ERROR_INVALID_ID;
    }
	
    max_id = slab_mgr->free_len - 1; 
    all_size = DFS_MATH_ALIGNMENT(size + sizeof(chunk_link_t),
        DFS_MATH_ALIGN_SIZE);
	
    if (all_size > slab_mgr->slabclass[max_id].size) 
	{
        return DFS_SLAB_ERROR_INVALID_ID;
    }
	
    if (all_size <= slab_mgr->slabclass[0].size) 
	{
        return 0;
    }
	
    if (all_size == slab_mgr->slabclass[max_id].size) 
	{
    	return max_id;
    }
    
    if (slab_mgr->uptype == DFS_SLAB_UPTYPE_LINEAR) 
	{
        id = (size - slab_mgr->min_size) / slab_mgr->factor;
		
        if (up == DFS_SLAB_TRUE && 
            (size - slab_mgr->min_size) % slab_mgr->factor) 
        {
            id++;
        }
		
        return id;
    }

    high = max_id - 1;
	
    while (low <= high) 
	{
        mid = (low + high) >> 1;
        if (all_size <= slab_mgr->slabclass[mid].size) 
		{
            if (all_size > slab_mgr->slabclass[mid - 1].size) 
			{
                return mid;
            } 
			else if (all_size == slab_mgr->slabclass[mid - 1].size) 
			{
                return mid - 1;
            }
			
            high = mid - 1;
        } 
		else if (all_size > slab_mgr->slabclass[mid].size) 
		{
            if (all_size <= slab_mgr->slabclass[mid + 1].size) 
			{
                return mid + 1;
            }
			
            low = mid + 1;
        }
    }
    
    return DFS_SLAB_ERROR_INVALID_ID;
}

dfs_slab_manager_t * dfs_slabs_create(dfs_mem_allocator_t *allocator, 
	                                       int uptype, size_t factor, 
	                                       const size_t item_size_min, 
	                                       const size_t item_size_max, 
	                                       dfs_slab_errno_t *err_no)
{
    ssize_t              i = 0;
    size_t               power = 0;
    size_t               size = item_size_min;
    ssize_t              free_len = 0;
    dfs_slab_manager_t  *slab_mgr = NULL;

    if (!err_no) 
	{
        return NULL;
    }
	
    DFS_SLAB_ERRNO_CLEAN(err_no);

    if (!item_size_min || !item_size_max || !allocator
        || item_size_max / item_size_min < 2) 
    {
        err_no->slab_errno = DFS_SLAB_ERR_CREATE_PARAM;
		
        return NULL;
    }
    
    if (uptype == DFS_SLAB_UPTYPE_POWER) 
	{
        if (factor != DFS_SLAB_POWER_FACTOR) 
		{
            err_no->slab_errno = DFS_SLAB_ERR_CREATE_POWER_FACTOR; 
			
            return NULL;
        }
		
        power = item_size_max / item_size_min;
        if (item_size_max % item_size_min) 
		{
            power++;
        }
		
        free_len = dfs_math_dfslog2(power, DFS_MATH_DFSLOG2_UP);
    } 
	else if (uptype == DFS_SLAB_UPTYPE_LINEAR) 
	{
        if (factor != DFS_SLAB_LINEAR_FACTOR) 
		{
            err_no->slab_errno = DFS_SLAB_ERR_CREATE_LINER_FACTOR;
			
            return NULL;
        }
		
        free_len = (ssize_t)((item_size_max - item_size_min) / factor);
        if ((item_size_max - item_size_min) % factor) 
		{
            free_len++;
        }
    } 
	else 
	{
        err_no->slab_errno = DFS_SLAB_ERR_CREATE_UPTYPE;
		
        return NULL;
    }
    
    free_len++;

    if (!(slab_mgr = (dfs_slab_manager_t *)allocator->calloc(allocator, sizeof(dfs_slab_manager_t),
        &err_no->allocator_errno))) 
    {
        err_no->slab_errno = DFS_SLAB_ERR_ALLOCATOR_ERROR;
		
        return NULL;
    }
    
    if (!(slab_mgr->slabclass = (slabclass_t *)allocator->calloc(allocator,
        sizeof(slabclass_t) * free_len, &err_no->allocator_errno))) 
    {
        err_no->slab_errno = DFS_SLAB_ERR_ALLOCATOR_ERROR;
		
        return NULL;
    }

    for (i = 0; i < free_len; i++) 
	{
        slab_mgr->slabclass[i].size = DFS_MATH_ALIGNMENT(
            size + sizeof(chunk_link_t), DFS_MATH_ALIGN_SIZE);
        if (uptype == DFS_SLAB_UPTYPE_POWER) 
		{
            size *= factor;
        } 
		else 
		{
            size += factor;
        }
    }
    
    slab_mgr->free_len = free_len;
    slab_mgr->factor = factor;
    slab_mgr->allocator = allocator;
    slab_mgr->uptype = uptype;
    slab_mgr->min_size = item_size_min;
    slab_mgr->slab_stat.system_size = sizeof(dfs_slab_manager_t)
        + sizeof(slabclass_t) * free_len;

    return slab_mgr;
}

int dfs_slabs_release(dfs_slab_manager_t **slab_mgr, 
	                       dfs_slab_errno_t *err_no)
{
    int                  i = 0;
    chunk_link_t        *chunk = NULL;
    dfs_mem_allocator_t *allocator = NULL;

    if (err_no) 
	{
        return DFS_SLAB_ERROR;
    }
	
    DFS_SLAB_ERRNO_CLEAN(err_no);

    if (!slab_mgr || !(*slab_mgr) || !(*slab_mgr)->allocator) 
	{
        err_no->slab_errno = DFS_SLAB_ERR_RELEASE_PARAM;
		
        return DFS_SLAB_ERROR;
    }

    allocator = (*slab_mgr)->allocator;
    for (i = 0; i < (*slab_mgr)->free_len; i++)
	{
        while((*slab_mgr)->slabclass[i].free_list) 
		{
            chunk = (*slab_mgr)->slabclass[i].free_list->next;
            allocator->free(allocator, (*slab_mgr)->slabclass[i].free_list,
                &err_no->allocator_errno);
            err_no->slab_errno = DFS_SLAB_ERR_ALLOCATOR_ERROR;
            (*slab_mgr)->slabclass[i].free_list = chunk;
        }
    }
    
    if (allocator->free(allocator, (*slab_mgr), &err_no->allocator_errno)
        == DFS_MEM_ALLOCATOR_ERROR) 
    {
        err_no->slab_errno = DFS_SLAB_ERR_ALLOCATOR_ERROR;
		
        return DFS_SLAB_ERROR;
    }
	
    *slab_mgr = NULL;

    return DFS_SLAB_OK;
}

static int dfs_slabs_recover(dfs_slab_manager_t *slab_mgr, 
	                               size_t chunk_size, int id)
{
    int                  i = 0;
    size_t               size = 0;
    int                  space_flag = DFS_SLAB_FALSE;
    chunk_link_t        *chunk = NULL;
    dfs_mem_allocator_t *allocator = NULL;
    unsigned int         allocator_errno = -1;

    allocator = slab_mgr->allocator;

    slab_mgr->slab_stat.recover++;
	
    for (i = id + 1; i < slab_mgr->free_len; i++) 
	{
        if (slab_mgr->slabclass[i].free_list) 
		{
            chunk = slab_mgr->slabclass[i].free_list->next;
            allocator->free(allocator, slab_mgr->slabclass[i].free_list,
                &allocator_errno);
            slab_mgr->slabclass[i].free_list = chunk;
            slab_mgr->slab_stat.free_size -= (slab_mgr->slabclass[i].size -
                sizeof(chunk_link_t));
            slab_mgr->slab_stat.chunk_count--;
            
            return DFS_SLAB_TRUE;
        }
    }
	
    for (i = id - 1; i >= 0; i--) 
	{
        if (slab_mgr->slabclass[i].free_list) 
		{
            space_flag = DFS_SLAB_TRUE;
        }
		
        while (slab_mgr->slabclass[i].free_list) 
		{
            chunk = slab_mgr->slabclass[i].free_list->next;
            allocator->free(allocator, slab_mgr->slabclass[i].free_list,
                &allocator_errno);
            slab_mgr->slabclass[i].free_list = chunk;
            slab_mgr->slab_stat.free_size -= (slab_mgr->slabclass[i].size -
                sizeof(chunk_link_t));
            slab_mgr->slab_stat.chunk_count--;
            size += (slab_mgr->slabclass[i].size);
			
            if (size >= chunk_size * DFS_SLAB_RECOVER_FACTOR) 
			{
                return DFS_SLAB_TRUE;
            }
        }
    }
	
    if (!space_flag) 
	{
        slab_mgr->slab_stat.recover_failed++;
		
        return DFS_SLAB_ERROR_NOSPACE;
    }
	
    slab_mgr->slab_stat.recover_failed++;
	
    return DFS_SLAB_FALSE;
}

void * dfs_slabs_alloc(dfs_slab_manager_t* slab_mgr, 
	                       int alloc_type,size_t req_size, 
	                       size_t *slab_size, dfs_slab_errno_t *err_no)
{
    chunk_link_t        *chunk = NULL;
    size_t               chunk_size = 0;
    ssize_t              id = 0;
    dfs_mem_allocator_t *allocator = NULL;
    int                  ret = 0;

    if (!err_no) 
	{
        return NULL;
    }
	
    DFS_SLAB_ERRNO_CLEAN(err_no);

    if (!slab_mgr || !slab_mgr->allocator || !req_size || !slab_size) 
	{
        err_no->slab_errno = DFS_SLAB_ERR_ALLOC_PARAM;
		
        return NULL;
    }
	
    if ((id = dfs_slabs_clsid(slab_mgr, req_size, DFS_SLAB_TRUE))
    	== DFS_SLAB_ERROR_INVALID_ID) 
    {
        err_no->slab_errno = DFS_SLAB_ERR_ALLOC_INVALID_ID;
		
        return NULL;
    }
    
    allocator = slab_mgr->allocator;
    *slab_size = req_size;

    chunk = slab_mgr->slabclass[id].free_list;
    chunk_size = slab_mgr->slabclass[id].size;

    if (!chunk) 
	{
        chunk = (chunk_link_t *)allocator->alloc(allocator, chunk_size,
             &err_no->allocator_errno);
        if (!chunk) 
		{
            err_no->slab_errno = DFS_SLAB_ERR_ALLOCATOR_ERROR;
        }
		
        while (!chunk && (ret = dfs_slabs_recover(slab_mgr, chunk_size, id))
            != DFS_SLAB_ERROR_NOSPACE) 
        {
            chunk = (chunk_link_t *)allocator->alloc(allocator, chunk_size,
                &err_no->allocator_errno);
            if (!chunk) 
			{
                err_no->slab_errno = DFS_SLAB_ERR_ALLOCATOR_ERROR;
            }
        }
		
        if (chunk) 
		{
            slab_mgr->slab_stat.chunk_count++;
            chunk->size = chunk_size - sizeof(chunk_link_t);
        }
    }
	else 
	{
        slab_mgr->slabclass[id].free_list = chunk->next;
        slab_mgr->slab_stat.free_size -= chunk->size;
    }
	
    if (chunk) 
	{
        chunk->id = id;
        if (alloc_type == DFS_SLAB_ALLOC_TYPE_ACT) 
		{
            *slab_size = chunk->size;
        }
        
        chunk->req_size = *slab_size;
        slab_mgr->slab_stat.reqs_size += *slab_size;
        slab_mgr->slab_stat.used_size += chunk->size;
        
        return (void*) (chunk + 1);
    }
	
    *slab_size = 0; 
    slab_mgr->slab_stat.failed++;
	
    if (ret == DFS_SLAB_ERROR_NOSPACE)
	{
        err_no->slab_errno = DFS_SLAB_ERR_ALLOC_FAILED;
    }
	
    return NULL;
}

void *dfs_slabs_split_alloc(dfs_slab_manager_t *slab_mgr, size_t req_size, 
	                             size_t *slab_size, size_t req_minsize, 
	                             dfs_slab_errno_t *err_no)
{
    chunk_link_t        *chunk = NULL;
    size_t               chunk_size = 0;
    void                *ptr = NULL;
    size_t               minsize = 0;
    size_t               shm_act_size = 0;
    dfs_mem_allocator_t *allocator = NULL;
    
    if (!err_no) 
	{
        return NULL;
    }
	
    DFS_SLAB_ERRNO_CLEAN(err_no);

    if (!slab_mgr || !slab_mgr->allocator || !req_size || !slab_size) 
	{
        err_no->slab_errno = DFS_SLAB_ERR_SPLIT_ALLOC_PARAM;
		
    	goto EF;
    }
	
    allocator = slab_mgr->allocator;
    if (!slab_mgr->allocator->split_alloc) 
	{
        err_no->slab_errno = DFS_SLAB_ERR_SPLIT_ALLOC_NOT_SUPPORTED;
		
        goto EF;
    }

    chunk_size = DFS_MATH_ALIGNMENT(req_size + sizeof(chunk_link_t),
        DFS_MATH_ALIGN_SIZE);

    if (chunk_size > slab_mgr->slabclass[slab_mgr->free_len - 1].size) 
	{
    	err_no->slab_errno = DFS_SLAB_ERR_SPLIT_ALLOC_CHUNK_SIZE_TOO_LARGE;
		
    	goto EF;
    }

    minsize = DFS_MATH_ALIGNMENT(req_minsize + sizeof(chunk_link_t),
        DFS_MATH_ALIGN_SIZE);

    chunk = (chunk_link_t *)allocator->split_alloc(allocator, &shm_act_size, minsize,
        &err_no->allocator_errno);
    if (!chunk) 
	{
        slab_mgr->slab_stat.split_failed++;
        err_no->slab_errno = DFS_SLAB_ERR_ALLOCATOR_ERROR;
        *slab_size = 0;
    } 
	else 
	{
        chunk->id = DFS_SLAB_SPLIT_ID;
        chunk->size = shm_act_size - sizeof(chunk_link_t);
    	slab_mgr->slab_stat.chunk_count++;
    	if (req_size >= chunk->size) 
		{
    	    chunk->req_size = chunk->size;
    	    slab_mgr->slab_stat.reqs_size += chunk->size;
    	} 
		else 
		{
    	    chunk->req_size = req_size;
    	    slab_mgr->slab_stat.reqs_size += req_size;
    	}
		
    	slab_mgr->slab_stat.used_size += chunk->size;
    	*slab_size = chunk->size;
    	ptr = (void *)(chunk + 1);
    }
    
EF:
    return ptr;
}

int dfs_slabs_free(dfs_slab_manager_t *slab_mgr,void *ptr, 
	                   dfs_slab_errno_t *err_no)
{
    chunk_link_t        *chunk = NULL;
    dfs_mem_allocator_t *allocator = NULL;

    if (!err_no) 
	{
        return DFS_SLAB_ERROR;
    }
	
    DFS_SLAB_ERRNO_CLEAN(err_no);

    if (!slab_mgr || !slab_mgr->allocator || !ptr) 
	{
        err_no->slab_errno = DFS_SLAB_ERR_FREE_PARAM;
		
        return DFS_SLAB_ERROR;
    }
	
    if (!slab_mgr->allocator->free) 
	{
        err_no->slab_errno = DFS_SLAB_ERR_FREE_NOT_SUPPORTED;
		
        return DFS_SLAB_OK;
    }
	
    allocator = slab_mgr->allocator;
    chunk = (chunk_link_t *)ptr - 1;
	
    if (chunk->id == DFS_SLAB_SPLIT_ID) 
	{
        slab_mgr->slab_stat.chunk_count--;
        slab_mgr->slab_stat.used_size -= chunk->size;
        slab_mgr->slab_stat.reqs_size -= chunk->req_size;
		
        if (allocator->free(allocator, chunk, &err_no->allocator_errno)
            == DFS_MEM_ALLOCATOR_ERROR) 
        {
            err_no->slab_errno = DFS_SLAB_ERR_ALLOCATOR_ERROR;
        }
            
        return DFS_SLAB_OK;
    }
	
    chunk->next = slab_mgr->slabclass[chunk->id].free_list;
    slab_mgr->slabclass[chunk->id].free_list = chunk;

    if (slab_mgr->slab_stat.used_size < slab_mgr->slabclass[chunk->id].size) 
    {
        err_no->slab_errno = DFS_SLAB_ERR_FREE_CHUNK_ID;
		
    	return DFS_SLAB_ERROR;
    }

    slab_mgr->slab_stat.free_size += chunk->size;
    slab_mgr->slab_stat.used_size -= chunk->size;
    slab_mgr->slab_stat.reqs_size -= chunk->req_size;
    
    return DFS_SLAB_OK;
}

size_t dfs_slabs_get_chunk_size(dfs_slab_manager_t *slab_mgr)
{
	return slab_mgr->slab_stat.chunk_count * sizeof(chunk_link_t);
}

const char * dfs_slabs_strerror(dfs_slab_manager_t *slab_mgr, 
	                                 const dfs_slab_errno_t *err_no)
{
    unsigned int slab_errno = -1;
    unsigned int allocator_errno = -1;

    if (!slab_mgr || !err_no) 
	{
        return dfs_slabs_err_string[0];
    }
	
    slab_errno = err_no->slab_errno;
    allocator_errno = err_no->allocator_errno;
    if (slab_errno <= DFS_SLAB_ERR_START || slab_errno >= DFS_SLAB_ERR_END) 
	{
        return dfs_slabs_err_string[0];
    }

    if (slab_errno == DFS_SLAB_ERR_ALLOCATOR_ERROR
        || slab_errno == DFS_SLAB_ERR_ALLOC_FAILED) 
    {
        if (slab_mgr->allocator && slab_mgr->allocator->strerror
            && allocator_errno) 
        {
            return slab_mgr->allocator->strerror(slab_mgr->allocator,
                &allocator_errno);
        }
    }

    return dfs_slabs_err_string[slab_errno - DFS_SLAB_ERR_START];
}

int dfs_slabs_get_stat(dfs_slab_manager_t *slab_mgr, dfs_slab_stat_t *stat)
{
    if (!slab_mgr || !stat) 
	{
        return DFS_SLAB_ERROR;
    }

    *stat = slab_mgr->slab_stat;
    stat->chunk_size = stat->chunk_count * sizeof(chunk_link_t);

    return DFS_SLAB_OK;
}

