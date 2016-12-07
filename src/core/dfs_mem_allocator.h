#ifndef DFS_MEM_ALLOCATOR_H
#define DFS_MEM_ALLOCATOR_H

#include <stdlib.h>

#include "dfs_error_log.h"

typedef struct dfs_mem_allocator_s dfs_mem_allocator_t;
typedef int   (*dfs_init_ptr_t)     (dfs_mem_allocator_t *me, void *param_data);
typedef int   (*dfs_release_ptr_t)  (dfs_mem_allocator_t *me, void *param_data);
typedef void* (*dfs_alloc_ptr_t)    (dfs_mem_allocator_t *me, size_t size,
    void *param_data);
typedef void* (*dfs_calloc_ptr_t)   (dfs_mem_allocator_t *me, size_t size,
    void *param_data);
typedef void* (*dfs_split_alloc_ptr_t) (dfs_mem_allocator_t *me, size_t *act_size,
    size_t req_minsize, void *param_data);
typedef int  (*dfs_free_ptr_t)     (dfs_mem_allocator_t *me, void *ptr,
    void *param_data);
typedef const char* (*dfs_strerror_ptr_t) (dfs_mem_allocator_t *me,
    void *err_data);
typedef int (*dfs_stat_ptr_t) (dfs_mem_allocator_t *me, void *stat_data);

struct dfs_mem_allocator_s 
{
    void                 *private_data;
    int                   type;
    dfs_init_ptr_t        init;
    dfs_release_ptr_t     release;
    dfs_alloc_ptr_t       alloc;
    dfs_calloc_ptr_t      calloc;
    dfs_split_alloc_ptr_t split_alloc;
    dfs_free_ptr_t        free;
    dfs_strerror_ptr_t    strerror;
    dfs_stat_ptr_t        stat;
    log_t                  *log;
};

enum
{
    DFS_MEM_ALLOCATOR_TYPE_SHMEM = 1,
    DFS_MEM_ALLOCATOR_TYPE_MEMPOOL,
    DFS_MEM_ALLOCATOR_TYPE_COMMPOOL,
};

#define DFS_MEM_ALLOCATOR_OK    (0)
#define DFS_MEM_ALLOCATOR_ERROR (-1)

extern const dfs_mem_allocator_t *dfs_get_mempool_allocator(void);
extern const dfs_mem_allocator_t *dfs_get_shmem_allocator(void);
extern const dfs_mem_allocator_t *dfs_get_commpool_allocator(void);
dfs_mem_allocator_t *dfs_mem_allocator_new(int allocator_type);
dfs_mem_allocator_t *dfs_mem_allocator_new_init(int allocator_type, 
	void *init_param);
void dfs_mem_allocator_delete(dfs_mem_allocator_t *allocator);

#endif

