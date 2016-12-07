#include "dfs_list.h"
#include "dfs_memory.h"

int list_init(list_t *list, pool_t *pool, uint32_t n, size_t size)
{
    if (list->part.elts) 
	{
        return DFS_ERROR;
    }
	
    if (pool) 
	{
        list->part.elts = pool_alloc(pool, n * size);
    } 
	else 
	{
        list->part.elts = memory_alloc(n * size);
    }
	
    if (!list->part.elts) 
	{
        return DFS_ERROR;
    }
	
    list->part.nelts = 0;
    list->part.next = NULL;
    list->last = &list->part;
    list->size = size;
    list->nalloc = n;
    list->pool = pool;

    return DFS_OK;
}

void list_reset(list_t *list)
{
    list_part_t *last = NULL;

    if (!list) 
	{
        return;
    }
	
    last = &list->part;
	
    while (last) 
	{
        last->nelts = 0;
        last = last->next;
    }
	
    list->last = &list->part;
}

void * list_push(list_t *l)
{
    void        *elt = NULL;
    list_part_t *last = NULL;

    last = l->last;

    while (last) 
	{
        l->last = last;
        if (last->nelts < l->nalloc) 
		{
            elt = (char *) last->elts + l->size * last->nelts;
            last->nelts++;
			
            return elt;
        }
		
        last = last->next;
    }
	
    // the last part is full, allocate a new list part
    if (l->pool) 
	{
        last = (list_part_t *)pool_alloc(l->pool, sizeof(list_part_t));
        last->elts = pool_alloc(l->pool, l->nalloc * l->size);
    } 
	else 
	{
        last = (list_part_t *)memory_alloc( sizeof(list_part_t));
        last->elts = memory_alloc(l->nalloc * l->size);
    }
	
    if (!last || !last->elts) 
	{
        return NULL;
    }
    
    last->nelts = 0;
    last->next = NULL;
    // add last to list_part's next
    l->last->next = last;
    // set last, use to alloc
    l->last = last;
    
    elt = (char *) last->elts + l->size * last->nelts;
    last->nelts++;

    return elt;
}

