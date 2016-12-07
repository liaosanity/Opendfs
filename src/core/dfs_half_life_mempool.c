#include <stdlib.h>

#include "dfs_half_life_mempool.h"
#include "dfs_queue.h"

struct hl_mempool_s
{
    int      max_size;
    int      min_size;
    int      element_size;
    int      free_size;
    queue_t  free_q;   
};

typedef struct hl_mem_node_s
{
    queue_t q;
    char    ptr[0];
} hl_mem_node_t;

static void do_clean(hl_mempool_t* pool);
static int do_expand(hl_mempool_t* pool);
static void do_shrink(hl_mempool_t* pool);

hl_mempool_t* hl_mempool_create(int max_reserv_size, 
	                                    int min_reserv_size, int elememt_size)
{
    hl_mempool_t  *t = NULL;
    hl_mem_node_t *node;
    int            alloc_size = 0;
	
    if (max_reserv_size <=0 || max_reserv_size< min_reserv_size 
		||elememt_size < 0)
	{
       return NULL; 
    }
	
    t= (hl_mempool_t*)malloc(sizeof(hl_mempool_t));
    if (!t) 
	{
        return NULL;
    }
	
    memset(t, 0x00, sizeof(hl_mempool_t));
    t->max_size = max_reserv_size;
    t->min_size = min_reserv_size;
    t->element_size = elememt_size;
    queue_init(&t->free_q);
    t->free_size = 0;
    alloc_size = sizeof(queue_t) + t->element_size;
	
    for (; t->free_size < t->max_size;)
    {
        node = (hl_mem_node_t *)malloc(alloc_size); 
        if (!node) 
		{
            do_clean(t);
            free(t);
			
            return NULL;
        }
		
        memset(node, 0x00, alloc_size);
        queue_insert_head(&t->free_q, &node->q);
        t->free_size++;
    }
    
    return t;
       
}

void * hl_mempool_get(hl_mempool_t* pool)
{
    int            ret = 0;
    queue_t       *queue = NULL;
    hl_mem_node_t *node = NULL;
	
    ret = do_expand(pool);
    if (ret != 0 && pool->free_size <= 0) 
	{
        return NULL;
    }
    
    queue = queue_head(&pool->free_q);
    if (!queue) 
	{
        return NULL;
    }
	
    queue_remove(queue);

    pool->free_size--;
    node = queue_data(queue, hl_mem_node_t, q);
	
    return node->ptr;
}

void hl_mempool_free(hl_mempool_t* pool, void* ptr)
{
    queue_t       *queue = NULL;
    hl_mem_node_t *node = NULL;

    node = queue_data(ptr, hl_mem_node_t, ptr);
    queue = &node->q;
    queue_insert_tail(&pool->free_q, queue);
    pool->free_size ++;
    do_shrink(pool);
}

int hl_mempool_get_free_size(hl_mempool_t* pool)
{
    return pool->free_size;
}

void hl_mempool_destroy(hl_mempool_t* pool)
{
    if (!pool) 
	{
        return;
    }
	
    do_clean(pool);
    free(pool);
}

static void do_clean(hl_mempool_t* pool)
{
    queue_t       *que = NULL;
    hl_mem_node_t *node = NULL;
	
    if (!pool) 
	{
        return;
    }
	
    while (!queue_empty(&pool->free_q)) 
	{
        que = queue_head(&pool->free_q);
        queue_remove(que);
        node = queue_data(que, hl_mem_node_t,q);
        free(node);
        pool->free_size--;
    }   
}

static int do_expand(hl_mempool_t* pool)
{
    hl_mem_node_t *node = NULL; 
    int            alloc_size = 0;
    
    if (pool->free_size > pool->min_size) 
	{
        return 0;
    }
	
    alloc_size = pool->element_size + sizeof(queue_t);
	
    for (; pool->free_size < pool->max_size;) 
	{
        node = (hl_mem_node_t *)malloc(alloc_size);
        if (!node) 
		{
            return -1;
        }
		
        memset(node, 0x00, alloc_size);
        queue_insert_tail(&pool->free_q, &node->q);
        pool->free_size++;
    } 
	
    return 0;
}

static void do_shrink(hl_mempool_t* pool)
{
    hl_mem_node_t *node = NULL; 
    queue_t       *que = NULL;
	
    if (pool->free_size <= pool->max_size) 
	{
        return;
    }
	
    while (pool->free_size > pool->max_size) 
	{
        que = queue_head(&pool->free_q);
        queue_remove(que);
        pool->free_size--;
        node = queue_data(que, hl_mem_node_t, q);
        free(node);
    }
}

