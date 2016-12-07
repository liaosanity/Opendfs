#ifndef DFS_SLABS_H
#define DFS_SLABS_H

#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/signal.h>
#include <sys/resource.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <pthread.h>

#include "dfs_shmem_allocator.h"

//#define DFS_SLAB_DEFAULT_MAX_SIZE ((size_t)10<<20)     // 10MB
#define DFS_SLAB_DEFAULT_MAX_SIZE  ((size_t)10485760) // 10MB
#define DFS_SLAB_DEFAULT_MIN_SIZE  ((size_t)1024)     // KB
#define DFS_SLAB_MAX_SIZE_PADDING  ((size_t)1<<20)    // MB

enum 
{
    DFS_SLAB_ERR_NONE = 0,
    DFS_SLAB_ERR_START = 100,
    DFS_SLAB_ERR_CREATE_PARAM,
    DFS_SLAB_ERR_CREATE_POWER_FACTOR,
    DFS_SLAB_ERR_CREATE_LINER_FACTOR,
    DFS_SLAB_ERR_CREATE_UPTYPE,
    DFS_SLAB_ERR_RELEASE_PARAM,
    DFS_SLAB_ERR_ALLOC_PARAM,
    DFS_SLAB_ERR_ALLOC_INVALID_ID,
    DFS_SLAB_ERR_ALLOC_FAILED,
    DFS_SLAB_ERR_SPLIT_ALLOC_PARAM,
    DFS_SLAB_ERR_SPLIT_ALLOC_NOT_SUPPORTED,
    DFS_SLAB_ERR_SPLIT_ALLOC_CHUNK_SIZE_TOO_LARGE,
    DFS_SLAB_ERR_FREE_PARAM,
    DFS_SLAB_ERR_FREE_NOT_SUPPORTED,
    DFS_SLAB_ERR_FREE_CHUNK_ID,
    DFS_SLAB_ERR_ALLOCATOR_ERROR,
    DFS_SLAB_ERR_END
};

#define DFS_SLAB_NO_ERROR                          0
#define DFS_SLAB_ERROR_INVALID_ID                 -1
#define DFS_SLAB_ERROR_NOSPACE                    -2
#define DFS_SLAB_ERROR_UNKNOWN                    -3
#define DFS_SLAB_ERROR_ALLOC_FAILED               -4
#define DFS_SLAB_SPLIT_ID                         -5

#define	DFS_SLAB_FALSE	            0
#define	DFS_SLAB_TRUE	            1
#define DFS_SLAB_NO_FREE            2

#define DFS_SLAB_ERROR              -1
#define DFS_SLAB_OK                 0

#define DFS_SLAB_RECOVER_FACTOR     2

#define DFS_SLAB_ALLOC_TYPE_REQ     0
#define DFS_SLAB_ALLOC_TYPE_ACT     1

#define DFS_SLAB_CHUNK_SIZE    sizeof(chunk_link_t)

enum 
{
    DFS_SLAB_UPTYPE_POWER = 1,
    DFS_SLAB_UPTYPE_LINEAR
};

enum 
{
    DFS_SLAB_POWER_FACTOR = 2,
    DFS_SLAB_LINEAR_FACTOR = 1024,
};

typedef struct chunk_link chunk_link_t;

struct chunk_link 
{
	size_t        size;
    size_t        req_size;
    ssize_t       id;
    chunk_link_t *next;
};

typedef struct slabclass_s 
{
    size_t        size;
    chunk_link_t *free_list;
} slabclass_t;

typedef struct dfs_slab_errno_s 
{
    unsigned int slab_errno;
    unsigned int allocator_errno;
} dfs_slab_errno_t;

typedef struct dfs_slab_stat_s
{
    size_t used_size;
    size_t reqs_size;
    size_t free_size;
    size_t chunk_count;
    size_t chunk_size;
    size_t system_size;
    size_t failed;
    size_t recover;
    size_t recover_failed;
    size_t split_failed;
} dfs_slab_stat_t;

typedef struct dfs_slab_manager_s 
{
    int                  uptype;    // increament type
    ssize_t              free_len;  // length of array slabclass
    size_t               factor;    // increament step
    size_t               min_size;  // the size of slabclass[0]
    dfs_slab_stat_t      slab_stat; // stat for slab
    dfs_mem_allocator_t *allocator; // the pointer to memory
    slabclass_t         *slabclass; // slab class array 0..free_len - 1
} dfs_slab_manager_t;

dfs_slab_manager_t * dfs_slabs_create(dfs_mem_allocator_t *allocator, 
	int uptype, size_t factor, const size_t item_size_min, 
	const size_t item_size_max, dfs_slab_errno_t *err_no);
int dfs_slabs_release(dfs_slab_manager_t **slab_mgr, dfs_slab_errno_t *err_no);
void * dfs_slabs_alloc(dfs_slab_manager_t *slab_mgr, int alloc_type, 
    size_t req_size, size_t *slab_size, dfs_slab_errno_t *err_no);
void * dfs_slabs_split_alloc(dfs_slab_manager_t *slab_mgr, size_t req_size,
    size_t *slab_size, size_t req_minsize, dfs_slab_errno_t *err_no);
int dfs_slabs_free(dfs_slab_manager_t *slab_mgr, void *ptr, 
	dfs_slab_errno_t *err_no);
const char * dfs_slabs_strerror(dfs_slab_manager_t *slab_mgr, 
	const dfs_slab_errno_t *err_no);
int dfs_slabs_get_stat(dfs_slab_manager_t *slab_mgr, dfs_slab_stat_t *stat);
size_t dfs_slabs_get_chunk_size(dfs_slab_manager_t *slab_mgr);

#endif

