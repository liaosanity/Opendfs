#ifndef DFS_MEMORY_POOL_H
#define DFS_MEMORY_POOL_H

#include "dfs_types.h"

#define DEFAULT_PAGESIZE       4096
#define DEFAULT_CACHELINE_SIZE 64

#define DFS_MAX_ALLOC_FROM_POOL ((size_t)(DEFAULT_PAGESIZE - 1))

#define dfs_align_ptr(p, a)       \
    (uchar_t *) (((uintptr_t)(p) + ((uintptr_t)(a) - 1)) & ~((uintptr_t)(a) - 1))

typedef struct pool_large_s  pool_large_t;

struct pool_large_s 
{
    void         *alloc;
    size_t        size;
    pool_large_t *next;
};

typedef struct pool_data_s 
{
    uchar_t *last;
    uchar_t *end;
    pool_t  *next;
} pool_data_t;

struct pool_s 
{
    pool_data_t   data;    // pool's list used status(used for pool alloc)
    size_t        max;     // max size pool can alloc
    pool_t       *current; // current pool of pool's list
    pool_large_t *large;   // large memory of pool
    log_t        *log;
};

pool_t *pool_create(size_t size, size_t max_size, log_t *log);
void    pool_destroy(pool_t *pool);
void   *pool_alloc(pool_t *pool, size_t size);
void   *pool_calloc(pool_t *pool, size_t size);
void   *pool_memalign(pool_t *pool, size_t size, size_t alignment);
void    pool_reset(pool_t *pool);
size_t  dfs_align(size_t d, uint32_t a);

#endif

