#include "dfs_shmem.h"
#include "dfs_math.h"
#include "dfs_memory.h"

static const char* dfs_shmem_err_string[] = 
{
    "dfs_shmem: unknown error number",
    "dfs_shmem_create: size error",
    "dfs_shmem_create: minsize is 0 or less than maxsize",
    "dfs_shmem_create: unknown leve type",
    "dfs_shmem_create: free len is 0",
    "dfs_shmem_create: total size error",
    "dfs_shmem_create: total size is not enough",
    "dfs_shmem_create: mmap failed",
    "dfs_shmem_create: storage size less than maxsize",
    "dfs_shmem_release: shmem instance is null",
    "dfs_shmem_release: munmap failed",
    "dfs_shmem_alloc: parameter error",
    "dfs_shmem_alloc: memory is exhausted",
    "dfs_shmem_alloc: max available free list is empty",
    "dfs_shmem_alloc: no available free list",
    "dfs_shmem_alloc: no fixed free space",
    "dfs_shmem_alloc: found no fixed free space",
    "dfs_shmem_alloc: remove free failed",
    "dfs_shmem_get_max_st: parameter error",
    "dfs_shmem_get_max_st: memory is exhausted",
    "dfs_shmem_get_max_st: critical error", 
    "dfs_shmem_split_alloc: parameter error",
    "dfs_shmem_split_alloc: max storage size less than request size",
    "dfs_shmem_split_alloc: remove free failed",
    "dfs_shmem_free: shmem instance or address is null",
    "dfs_shmem_free: free a non-alloced address",
    "dfs_shmem_free: remove the next storage failed",
    "dfs_shmem_free: remove the previous storage failed",
    NULL
};

size_t dfs_shmem_get_storage_size(dfs_shmem_t *shm)
{
	return shm->shmem_stat.st_count * sizeof(struct storage);
}

int dfs_shmem_set_split_threshold(dfs_shmem_t *shm, size_t size)
{
	if (!shm) 
	{
		return DFS_SHMEM_ERROR;
    }
	
    shm->split_threshold = size;
	
    return DFS_SHMEM_OK;
}

size_t dfs_shmem_get_system_size(dfs_shmem_t *shm)
{
	return shm ? shm->shmem_stat.system_size : 0;
}

size_t dfs_shmem_get_total_size(dfs_shmem_t *shm)
{
	return shm ? shm->shmem_stat.total_size : 0;
}

size_t dfs_shmem_get_used_size(dfs_shmem_t *shm)
{
	return shm ? shm->shmem_stat.used_size : 0;
}

static size_t dfs_shmem_get_alloc_index(dfs_shmem_t *shm, size_t size)
{
    size_t id = 0;
    size_t power = 0;

    if (size <= shm->min_size) 
	{
    	return 0;
    }

    if (shm->level_type == DFS_SHMEM_LEVEL_TYPE_LINEAR) 
	{
    	id = (size - shm->min_size) / shm->factor;
        if ((size - shm->min_size) % shm->factor) 
		{
            id++;
        }
    } 
	else 
	{
        power = size / shm->min_size;
		
        if (size % shm->min_size) 
		{
            power++;
        }
		
        id = (size_t)dfs_math_dfslog2(power, DFS_MATH_DFSLOG2_UP);
    }
    
    return id;
}

static size_t dfs_shmem_get_insert_index(dfs_shmem_t *shm, size_t size)
{
    size_t id = 0;
    size_t power  = 0;

    if (size <= shm->min_size)
	{
    	return 0;
    }

    if (shm->level_type == DFS_SHMEM_LEVEL_TYPE_LINEAR) 
	{
    	id = (size - shm->min_size) / shm->factor;
    } 
	else 
	{
        power = size / shm->min_size;
        id = (size_t)dfs_math_dfslog2(power, DFS_MATH_DFSLOG2_DOWN);
    }
    
    return id;
}

dfs_shmem_t * dfs_shmem_create(size_t size, size_t min_size, 
	                                   size_t max_size, int level_type, 
	                                   size_t factor, unsigned int *shmem_errno)
{
    dfs_shmem_t    *shm = NULL;
    size_t          free_len = 0;
    size_t          power = 0;
    size_t          total_size = 0;
    size_t          i = 0;
    size_t          system_size = 0;
    size_t          free_size = 0;
    struct storage *st = NULL;
    
    *shmem_errno = DFS_SHMEM_ERR_NONE;
	
    if (!size) 
	{
        *shmem_errno = DFS_SHMEM_ERR_CREATE_SIZE;
		
        return NULL;
    }
    
    min_size = DFS_MATH_ALIGNMENT(min_size, DFS_MATH_ALIGN_SIZE);
    max_size = DFS_MATH_ALIGNMENT(max_size, DFS_MATH_ALIGN_SIZE);
    if (!min_size || min_size >= max_size) 
	{
        *shmem_errno = DFS_SHMEM_ERR_CREATE_MINSIZE;
		
    	return NULL;
    }
    
    if (level_type == DFS_SHMEM_LEVEL_TYPE_LINEAR) 
	{
        factor = DFS_MATH_ALIGNMENT(factor, DFS_MATH_ALIGN_SIZE);
	    free_len = (max_size - min_size) / factor;
        if ((max_size - min_size) % factor) 
		{
            free_len++;
        }
    } 
	else if (level_type == DFS_SHMEM_LEVEL_TYPE_EXP) 
	{
        factor = DFS_SHMEM_EXP_FACTOR;
        power = max_size / min_size;
        if (max_size % min_size) 
		{
            power++;
        }
		
	    free_len = dfs_math_dfslog2(power, 1);
    } 
	else 
	{
        *shmem_errno = DFS_SHMEM_ERR_CREATE_LEVELTYPE;
		
	    return NULL;
    }
	
    if (!free_len) 
	{
        *shmem_errno = DFS_SHMEM_ERR_CREATE_FREELEN;
		
    	return NULL;
    }
    
    free_len++;
	
    total_size = DFS_MATH_ROUND_UP(size, DFS_PAGE_SIZE);
    if (!total_size) 
	{
        *shmem_errno = DFS_SHMEM_ERR_CREATE_TOTALSIZE;
		
        return NULL;
    }
	
    shm = (dfs_shmem_t*)mmap(NULL, total_size,
        PROT_READ|PROT_WRITE, MAP_ANON|MAP_SHARED, -1, 0);

    if ((void*)shm == MAP_FAILED) 
	{
        *shmem_errno = DFS_SHMEM_ERR_CREATE_MMAP;
		
        return NULL;
    }
   
    shm->split_threshold = min_size;
    shm->min_size = min_size;
    shm->max_size = max_size;
    shm->factor = factor; 
    shm->free_len = free_len;
    shm->level_type = level_type;

    free_size = sizeof(struct free_node) * shm->free_len;
    system_size = sizeof(dfs_shmem_t);
    if (free_size + system_size + sizeof(struct storage) >= total_size) 
	{
        munmap(shm, total_size);
        *shmem_errno = DFS_SHMEM_ERR_CREATE_TOTALSIZE_NOT_ENOUGH;
		
        return NULL;
    }
	
    shm->free = (struct free_node *)((char *)shm + system_size);
    for (i = 0; i < shm->free_len; i++) 
	{
        shm->free[i].index = i;
        queue_init(&shm->free[i].free_list_head);
    }

    system_size += free_size;
    system_size = DFS_MATH_ALIGNMENT(system_size, DFS_MATH_ALIGN_SIZE);
    shm->shmem_stat.total_size = total_size;
    shm->shmem_stat.reqs_size = 0;
    shm->shmem_stat.st_count = 1;
    shm->shmem_stat.used_size = 0;
    shm->shmem_stat.system_size = system_size;

    st = (struct storage *)((unsigned char *)shm + system_size);
    st->size = total_size - system_size - sizeof(struct storage);
    st->alloc = 0;
    if (st->size < max_size) 
	{
        munmap(shm, total_size);
        *shmem_errno = DFS_SHMEM_ERR_CREATE_STORAGESIZE;
		
        return NULL;
    }

    queue_init(&shm->order);
    queue_insert_head(&shm->order, &st->order_entry);
    
    i = dfs_shmem_get_insert_index(shm, st->size);
    if (i >= shm->free_len) 
	{
    	i = shm->free_len - 1;
    }
	
    st->free_list_head = &shm->free[i].free_list_head;
    queue_insert_head(&shm->free[i].free_list_head, &st->free_entry);

    queue_init(&shm->available);
    queue_insert_head(&shm->available, &shm->free[i].available_entry);
    shm->max_available_index = i;
    
    return shm;
}

int dfs_shmem_release(dfs_shmem_t **shm, unsigned int *shmem_errno)
{
    *shmem_errno = DFS_SHMEM_ERR_NONE;
	
    if (!shm || !*shm) 
	{
        *shmem_errno = DFS_SHMEM_ERR_RELEASE_NULL;
		
        return DFS_SHMEM_ERROR;
    }
	
    if (!munmap(*shm, (*shm)->shmem_stat.total_size)) 
	{
        *shm = NULL;
        return DFS_SHMEM_OK;
    }
	
    *shmem_errno = DFS_SHMEM_ERR_RELEASE_MUNMAP;
	
    return DFS_SHMEM_ERROR;
}

static int dfs_shmem_remove_free(dfs_shmem_t *shm, struct storage *st)
{
    size_t            index = 0;
    queue_t          *pqueue = NULL;
    struct free_node *free = NULL;

    index = dfs_shmem_get_insert_index(shm, st->size);
    if (index > shm->max_available_index)
	{
        index = shm->max_available_index;
    }
	
    if (st->free_list_head != &shm->free[index].free_list_head)
	{
        return DFS_SHMEM_ERROR;
    }
	
    queue_remove(&st->free_entry);

    if (queue_empty(&shm->free[index].free_list_head)) 
	{
    	if (index == shm->max_available_index) 
		{
    	    pqueue = queue_prev(&shm->free[index].available_entry);
    	    if (pqueue == &shm->available) 
			{
    	        shm->max_available_index = shm->free_len;	
            } 
			else 
			{
                free = queue_data(pqueue, struct free_node, available_entry);
                shm->max_available_index = free->index;
            }
        }
		
        queue_remove(&shm->free[index].available_entry);
    }

    st->free_list_head = NULL;

    return DFS_SHMEM_OK;
}

static void dfs_shmem_insert_free(dfs_shmem_t *shm, 
	                                      struct storage *st_free)
{
    struct free_node *free1 = NULL;
    struct free_node *free = NULL;
    queue_t          *q = NULL;
    size_t            index = 0;
    int               flag = 1;

    if (!st_free || st_free->alloc) 
	{
        return;
    }

    index = dfs_shmem_get_insert_index(shm, st_free->size);
    if (index > shm->free_len - 1) 
	{
        index = shm->free_len - 1;
    }
	
    st_free->free_list_head = &shm->free[index].free_list_head;    
    free = &shm->free[index];    

    if (queue_empty(&shm->free[index].free_list_head)) 
	{
        q = queue_head(&shm->available);
		
        while(1) 
		{
            if (q == &shm->available) 
			{
                break;
            }
			
            free1 = queue_data(q, struct free_node, available_entry);
			
            if (index < free1->index) 
			{
                queue_insert_before(&free1->available_entry,
                    &free->available_entry);
                flag = 0;
                break;
            }
			
            q = queue_next(q);
        }
		
        if (flag) 
		{
            queue_insert_tail(&shm->available, &free->available_entry);
            shm->max_available_index = free->index;
        }
    }
	
    queue_insert_head(&shm->free[index].free_list_head, &st_free->free_entry);
}

void * dfs_shmem_alloc(dfs_shmem_t *shm, size_t size, 
	                         unsigned int *shmem_errno)
{
    struct storage   *st = NULL;
    struct storage   *st_new = NULL;
    struct free_node *free = NULL;
    queue_t          *q = NULL;
    queue_t          *st_q = NULL;
    int               found_st = 0;
    int               no_free = 1;
    size_t            index = 0;

    *shmem_errno = DFS_SHMEM_ERR_NONE;
	
    if (!shm || !size) 
	{
        *shmem_errno = DFS_SHMEM_ERR_ALLOC_PARAM;
		
        return NULL;
    }

    if (shm->max_available_index == shm->free_len) 
	{
        *shmem_errno = DFS_SHMEM_ERR_ALLOC_EXHAUSTED;
        shm->shmem_stat.failed++;
		
        return NULL;
    }

    index = dfs_shmem_get_alloc_index(shm, size);
    
    if (index < shm->max_available_index) 
	{
        free = &shm->free[index];    
        if (!queue_empty(&free->free_list_head)) 
		{
        	goto DO_COMPARE;
        }
		
        q = queue_head(&shm->available);
		
        while (q != &shm->available) 
		{
            free = queue_data(q, struct free_node, available_entry);
			
            if (!queue_empty(&free->free_list_head) && free->index > index) 
			{
            	no_free = 0;
                break;
            }
			
            q = queue_next(&free->available_entry);
        }
    }
	else 
	{
        free = &shm->free[shm->max_available_index];
        if (queue_empty(&free->free_list_head)) 
		{
            *shmem_errno = DFS_SHMEM_ERR_ALLOC_MAX_AVAILABLE_EMPTY;
            shm->shmem_stat.failed++;
			
            return NULL;
        }
		
        goto DO_COMPARE;
    }
	
    if (no_free) 
	{
        *shmem_errno = queue_empty(&shm->available) ?
    	    DFS_SHMEM_ERR_ALLOC_NO_AVAILABLE_FREE_LIST :
    	    DFS_SHMEM_ERR_ALLOC_NO_FIXED_FREE_SPACE;
        shm->shmem_stat.failed++;
		
        return NULL;
    }

DO_COMPARE:
    st_q = queue_head(&free->free_list_head);
	
    while (1) 
	{
        st = queue_data(st_q, struct storage, free_entry);
        if (st->size >= size) 
		{
            found_st = 1;
			
            break;
        }
		
        st_q = queue_next(st_q);
        if (&free->free_list_head == st_q) 
		{
            break;
        }
    }
	
    if (!found_st) 
	{
        *shmem_errno = DFS_SHMEM_ERR_ALLOC_FOUND_NO_FIXED;
        shm->shmem_stat.failed++;
		
        return NULL;
    }
	
    if (dfs_shmem_remove_free(shm, st) == DFS_SHMEM_ERROR) 
	{
        *shmem_errno = DFS_SHMEM_ERR_ALLOC_REMOVE_FREE;
        shm->shmem_stat.failed++;
		
        return NULL;
    }

    if (st->size - size < sizeof(struct storage)
        + shm->split_threshold) 
    {
        st->alloc = 1;
        st->act_size = size;
        shm->shmem_stat.used_size += st->size;
        shm->shmem_stat.reqs_size += size;
		
        return (void*)((unsigned char*)st + sizeof(struct storage));
    }

    st_new = (struct storage *)((unsigned char *)st
        + sizeof(struct storage) + size);
    *st_new = *st;
    st_new->size -= (size + sizeof(struct storage));
    
    st->alloc = 1;
    st->size = size;

    queue_insert_before(&st_new->order_entry, &st->order_entry);
    queue_remove(&st_new->order_entry);
    queue_insert_after(&st->order_entry, &st_new->order_entry);

    dfs_shmem_insert_free(shm, st_new);
    st->act_size = size;
    shm->shmem_stat.used_size += st->size;
    shm->shmem_stat.reqs_size += size; 
    shm->shmem_stat.st_count++;
    
    return (void *)((char *)st + sizeof(struct storage));
}

void * dfs_shmem_calloc(dfs_shmem_t *shm, size_t size, 
	                           unsigned int *shmem_errno)
{
    void *p = dfs_shmem_alloc(shm, size, shmem_errno);
    
    if (p) 
	{
        memset(p, 0, size);
    }
    
    return p;
}

static struct storage * dfs_shmem_get_max_st(dfs_shmem_t *shm, 
	                                                   unsigned int *shmem_errno)
{
    queue_t          *q = NULL;
    queue_t          *st_q = NULL;
    struct free_node *free = NULL;
    struct storage   *st = NULL;
    struct storage   *st_max = NULL;

    *shmem_errno = DFS_SHMEM_ERR_NONE;

    if (!shm) 
	{
        *shmem_errno = DFS_SHMEM_ERR_GET_MAX_PARAM;
		
        return NULL;
    }

    if (shm->max_available_index == shm->free_len
    	|| queue_empty(&shm->available)) 
    {
    	*shmem_errno = DFS_SHMEM_ERR_GET_MAX_EXHAUSTED;
		
        return NULL;
    }

    q = queue_tail(&shm->available);
    free = queue_data(q, struct free_node, available_entry);
	
    if (queue_empty(&free->free_list_head)) 
	{
    	*shmem_errno = DFS_SHMEM_ERR_GET_MAX_CRITICAL;
		
        return NULL;
    }
	
    st_q = queue_head(&free->free_list_head);
    st_max = queue_data(st_q, struct storage, free_entry); 
    st_q = queue_next(st_q);
	
    while(st_q != &free->free_list_head) 
	{
        st = queue_data(st_q, struct storage, free_entry);
		
        if (st->size > st_max->size) 
		{
            st_max = st;
        }
		
        st_q = queue_next(st_q);
    }
	
    return st_max;
}

void * dfs_shmem_split_alloc(dfs_shmem_t *shm, size_t *act_size, 
	                                 size_t req_minsize, 
	                                 unsigned int *shmem_errno)
{
    struct storage *st = NULL;

    *shmem_errno = DFS_SHMEM_ERR_NONE;
    if (!act_size) 
	{
        *shmem_errno = DFS_SHMEM_ERR_SPLIT_ALLOC_PARAM;
		
        return NULL;
    }

    shm->shmem_stat.split++;
    st = dfs_shmem_get_max_st(shm, shmem_errno);
    if (!st) 
	{
        shm->shmem_stat.split_failed++;
		
    	return NULL;
    }
	
    if (st->size < req_minsize)
	{
        *shmem_errno = DFS_SHMEM_ERR_SPLIT_ALLOC_NO_FIXED_REQ_MINSIZE;
        shm->shmem_stat.split_failed++;
		
        return NULL;
    }
	
    if (dfs_shmem_remove_free(shm, st) == DFS_SHMEM_ERROR) 
	{
        *shmem_errno = DFS_SHMEM_ERR_SPLIT_ALLOC_REMOVE_FREE;
        shm->shmem_stat.split_failed++;
		
    	return NULL;
    }
	
    st->alloc = 1;
    *act_size = st->size;
    st->act_size = *act_size;
    shm->shmem_stat.used_size += *act_size;
    shm->shmem_stat.reqs_size += *act_size;
	
    return (void *)((char *)st + sizeof(struct storage));
}

void dfs_shmem_free(dfs_shmem_t *shm, void *addr, 
	                       unsigned int *shmem_errno)
{
    struct storage *st_alloc = NULL, *st = NULL;
    queue_t        *q = NULL;
    
    *shmem_errno = DFS_SHMEM_ERR_NONE;
	
    if (!shm || !addr) 
	{
        *shmem_errno = DFS_SHMEM_ERR_FREE_PARAM;
		
        return;
    }

    st_alloc = (struct storage*)((char *)addr - sizeof(struct storage));
    if (!st_alloc->alloc) 
	{
        *shmem_errno = DFS_SHMEM_ERR_FREE_NONALLOCED;
		
        return;
    }

    shm->shmem_stat.used_size -= st_alloc->size;
    shm->shmem_stat.reqs_size -= st_alloc->act_size;
    st_alloc->alloc = 0;

    q = queue_next(&st_alloc->order_entry);
    if (q == &shm->order) 
	{
        goto PROCESS_PREV;
    }
	
    st = queue_data(q, struct storage, order_entry);    
    if (st && !st->alloc
        && st == (struct storage *)((char *)st_alloc +
        sizeof(struct storage) + st_alloc->size)) 
    {
        if (dfs_shmem_remove_free(shm, st) == DFS_SHMEM_ERROR) 
		{
            *shmem_errno = DFS_SHMEM_ERR_FREE_REMOVE_NEXT;
			
            return;
        }
		
        st_alloc->size += st->size + sizeof(struct storage);
        queue_remove(&st->order_entry);
        shm->shmem_stat.st_count--;
    }
	
PROCESS_PREV:
    q = queue_prev(&st_alloc->order_entry);
    if (q == &shm->order) 
	{
        goto END;
    }
	
    st = queue_data(q, struct storage, order_entry);
    if (st && !st->alloc
        && st_alloc == (struct storage*)((unsigned char*)st +
        st->size + sizeof(struct storage))) 
    {
        if (dfs_shmem_remove_free(shm, st) == DFS_SHMEM_ERROR) 
		{
            *shmem_errno = DFS_SHMEM_ERR_FREE_REMOVE_PREV;
			
            return;
        }
		
        st->size += (st_alloc->size + sizeof(struct storage));
        queue_remove(&st_alloc->order_entry);
        st_alloc = st;
        shm->shmem_stat.st_count--;
    }
	
END:
    dfs_shmem_insert_free(shm, st_alloc);    
}

void dfs_shmem_dump(dfs_shmem_t *shm, FILE *debug_out)
{
}
const char* dfs_shmem_strerror(unsigned int shmem_errno)
{
    if (shmem_errno <= DFS_SHMEM_ERR_START
        || shmem_errno >= DFS_SHMEM_ERR_END) 
    {
        return dfs_shmem_err_string[0];
    }
	
    return dfs_shmem_err_string[shmem_errno - DFS_SHMEM_ERR_START];
}

int dfs_shmem_get_stat(dfs_shmem_t *shmem, dfs_shmem_stat_t *stat)
{
    if (!shmem || !stat) 
	{
        return DFS_SHMEM_ERROR;
    }

    *stat = shmem->shmem_stat;
    stat->st_size = stat->st_count * sizeof(struct storage);
	
    return DFS_SHMEM_OK;
}

uchar_t * dfs_shm_strdup(dfs_shmem_t *shm, string_t *str)
{
    unsigned int  error = -1;
    uchar_t      *dst = NULL;

    if (!str) 
	{
        return NULL;
    }
    
    dst = (uchar_t *)dfs_shmem_calloc(shm, str->len + 1, &error);
    if (!dst) 
	{
        return NULL;
    }
    
    memory_memcpy(dst, str->data, str->len);
    
    dst[str->len] = 0;
    
    return dst;
}

