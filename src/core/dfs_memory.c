#include "dfs_memory.h"

void * memory_alloc(size_t size)
{
    void *p = NULL;
    
    if (size == 0) 
	{
        return NULL;
    }
	
    p = malloc(size);   
	
    return p;
}

void * memory_calloc(size_t size)
{
    void *p = memory_alloc(size);

    if (p) 
	{
        memory_zero(p, size);
    }
    
    return p;
}

void memory_free(void *p, size_t size)
{
    if (p) 
	{
        free(p);
    }
}

void * memory_memalign(size_t alignment, size_t size)
{
    void *p = NULL;

    if (size == 0) 
	{
        return NULL;
    }
	
    posix_memalign(&p, alignment, size);
    
    return p;
}

int memory_n2cmp(uchar_t *s1, uchar_t *s2, size_t n1, size_t n2)
{
    size_t n = 0;
    int    m = 0, z = 0;

    if (n1 <= n2) 
	{
        n = n1;
        z = -1;
    } 
	else 
	{
        n = n2;
        z = 1;
    }
	
    m = memory_memcmp(s1, s2, n);
    if (m || n1 == n2) 
	{
        return m;
    }
	
    return z;
}

