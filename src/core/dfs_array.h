#ifndef DFS_ARRAY_H
#define DFS_ARRAY_H

#include "dfs_types.h"
#include "dfs_string.h"

struct array_s 
{
    void     *elts;
    uint32_t  nelts;
    size_t    size;
    uint32_t  nalloc;
    pool_t   *pool;
};

typedef int (*array_cmp_func)(void *, void *);
typedef void (*func_inherit)(void *, void *,void *);
typedef int (*func_cmp)(void *, void *);

void *array_create(pool_t *p, uint32_t n, size_t size);
int   array_init(array_t *array, pool_t *pool, uint32_t n, size_t size);
void  array_reset(array_t *a);
void  array_destroy(array_t *a);
void *array_push(array_t *a);
void *array_find(array_t *array, void* dst, array_cmp_func);

#endif

