#ifndef DFS_LIST_H
#define DFS_LIST_H

#include "dfs_types.h"
#include "dfs_memory_pool.h"

typedef struct list_part_s list_part_t;

struct list_part_s 
{
    void         *elts;
    uint32_t      nelts;
    list_part_t  *next;
};

typedef struct 
{
    list_part_t   part;   // used for select
    list_part_t  *last;   // used for alloc
    size_t        size;
    uint32_t      nalloc; // every part alloc number
    pool_t       *pool;
} list_t;

int   list_init(list_t *list, pool_t *pool, uint32_t n, size_t size);
void  list_reset(list_t *list);
void *list_push(list_t *list);

#endif

