#include "dfs_array.h"
#include "dfs_memory.h"

void * array_create(pool_t *p, uint32_t n, size_t size)
{
    array_t *a = NULL;

    if ((size == 0) || (n == 0)) 
	{
        return NULL;
    }
    
    if (p) 
	{
        a = (array_t *)pool_alloc(p, sizeof(array_t));
        a->elts = pool_alloc(p, n * size);
    } 
	else 
	{
        a = (array_t *)memory_alloc(sizeof(array_t));
        a->elts = memory_alloc(n * size);
    }
    
    if (!a || !(a->elts)) 
	{
        return NULL;
    }

    a->nelts = 0;
    a->size = size;
    a->nalloc = n;
    a->pool = p;

    return a;
}

int array_init(array_t *array, pool_t *pool, uint32_t n, size_t size)
{
    if (!array || (n == 0) || (size == 0)) 
	{
        return DFS_ERROR;
    }
	
    array->nelts = 0;
    array->size = size;
    array->nalloc = n;
    array->pool = pool;
	
    if (pool) 
	{
        array->elts = pool_alloc(pool, n * size);
    } 
	else
	{
        array->elts = memory_alloc(n * size);
    }
	
    if (!array->elts) 
	{
        return DFS_ERROR;
    }
    
    return DFS_OK;
}

void array_reset(array_t *a)
{
    memset(a->elts, 0, a->size * a->nelts);
    a->nelts = 0;
}

void array_destroy(array_t *a)
{
    pool_t *p = NULL;

    if (!a) 
	{
        return;
    }
    
    p = a->pool;
    
    if (!p) 
	{
        memory_free(a->elts,a->size * a->nalloc);
		
        return;
    }
    
    if ((uchar_t *) a->elts + a->size * a->nalloc == p->data.last) 
	{
        p->data.last = (uchar_t *)a->elts;
    }

    if ((uchar_t *) a + sizeof(array_t) == p->data.last) 
	{
        p->data.last = (uchar_t *) a;
    }
}

void * array_push(array_t *a)
{
    void   *elt = NULL;
    void   *na = NULL;
    size_t  size = 0;
    pool_t *p = NULL;

    if (!a) 
	{
        return NULL;
    }
    
    p = a->pool;
	
    // the array is full, note: array need continuous space
    if (a->nelts == a->nalloc) 
	{
        size = a->size * a->nalloc;
		
        if (!p) 
		{
            return NULL;
        }
		
        // the array allocation is the last in the pool
        // and there is continuous space for it
        if ((uchar_t *) a->elts + size == p->data.last
            && p->data.last + a->size <= p->data.end) 
        {
            p->data.last += a->size;
            a->nalloc++;
        } 
		else 
		{
            // allocate a new array
            if (p) 
			{
                na = pool_alloc(p, 2 * size);
                if (!na) 
				{
                    return NULL;
                }
				
                memory_memcpy(na, a->elts, size);
                a->elts = na;
                a->nalloc *= 2;
            } 
			else 
			{
                na = memory_realloc(a->elts, 2 * size);
                if (!na) 
				{
                    return NULL;
                }
				
                a->elts = na;
                a->nalloc *= 2;
            }
        }
    }
	
    elt = (uchar_t *) a->elts + a->size * a->nelts;
    a->nelts++;
    
    return elt;
}

void array_inherit(void *pool, array_t *a1, array_t *a2, 
	                  func_cmp func, func_inherit func2)
{
    size_t i = 0, j = 0;
    void  *v1 = NULL, *v2 = NULL;

    for (i = 0; i < a1->nelts; i++) 
	{
        v1 = (void *) ((char *)a1->elts + a1->size * i);
		
        for (j = 0; j < a2->nelts; j++)
		{
            v2 = (void *) ((char *)a2->elts + a2->size * i);
			
            if (func(v1, v2)) 
			{
                func2(pool, v1, v2);
				
				break;
            }
        }
	}
}

void * array_find(array_t *array, void *dst, array_cmp_func func)
{
	size_t i = 0;
	void  *v = NULL; 
	
	for (i = 0; i < array->nelts; i++) 
	{
		v = (void*)((char *)array->elts + array->size * i);
		
		if (func(v, dst)) 
		{
            return v;
        }
    }
	
    return NULL;
}

