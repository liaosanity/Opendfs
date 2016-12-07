#ifndef DFS_SHMEM_H
#define DFS_SHMEM_H

#include <sys/mman.h>
#include <sys/queue.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>

#include "dfs_queue.h"
#include "dfs_string.h"

#define DFS_SHMEM_DEFAULT_MAX_SIZE  ((size_t)10<<20) // 10MB
#define DFS_SHMEM_DEFAULT_MIN_SIZE  ((size_t)1024)

#define DFS_SHMEM_STORAGE_SIZE sizeof(struct storage)

#define DFS_SHMEM_INVALID_SPLIT_THRESHOLD       (0)
#define DFS_SHMEM_DEFAULT_SPLIT_THRESHOLD       (64)
#define DFS_SHMEM_MEM_LEVEL                     (32)
#define DFS_SHMEM_LEVEL_TYPE_LINEAR             (0)
#define DFS_SHMEM_LEVEL_TYPE_EXP                (1)
#define DFS_SHMEM_EXP_FACTOR                     2
#define DFS_SHMEM_LINEAR_FACTOR                  1024

#define DEFAULT_ALIGNMENT_MASK  (1)
#define DEFAULT_ALIGNMENT_SIZE  (1<<DEFAULT_ALIGNMENT_MASK)

#define DFS_SHMEM_ERROR                    (-1)
#define DFS_SHMEM_OK                       (0)

enum 
{
    DFS_SHMEM_ERR_NONE = 0,
    DFS_SHMEM_ERR_START = 100,
    DFS_SHMEM_ERR_CREATE_SIZE,
    DFS_SHMEM_ERR_CREATE_MINSIZE,
    DFS_SHMEM_ERR_CREATE_LEVELTYPE,
    DFS_SHMEM_ERR_CREATE_FREELEN,
    DFS_SHMEM_ERR_CREATE_TOTALSIZE,
    DFS_SHMEM_ERR_CREATE_TOTALSIZE_NOT_ENOUGH,
    DFS_SHMEM_ERR_CREATE_MMAP,
    DFS_SHMEM_ERR_CREATE_STORAGESIZE,
    DFS_SHMEM_ERR_RELEASE_NULL,
    DFS_SHMEM_ERR_RELEASE_MUNMAP,
    DFS_SHMEM_ERR_ALLOC_PARAM,
    DFS_SHMEM_ERR_ALLOC_EXHAUSTED,
    DFS_SHMEM_ERR_ALLOC_MAX_AVAILABLE_EMPTY,
    DFS_SHMEM_ERR_ALLOC_NO_AVAILABLE_FREE_LIST,
    DFS_SHMEM_ERR_ALLOC_NO_FIXED_FREE_SPACE,
    DFS_SHMEM_ERR_ALLOC_FOUND_NO_FIXED,
    DFS_SHMEM_ERR_ALLOC_REMOVE_FREE,
    DFS_SHMEM_ERR_GET_MAX_PARAM,
    DFS_SHMEM_ERR_GET_MAX_EXHAUSTED,
    DFS_SHMEM_ERR_GET_MAX_CRITICAL,
    DFS_SHMEM_ERR_SPLIT_ALLOC_PARAM,
    DFS_SHMEM_ERR_SPLIT_ALLOC_NO_FIXED_REQ_MINSIZE,
    DFS_SHMEM_ERR_SPLIT_ALLOC_REMOVE_FREE,
    DFS_SHMEM_ERR_FREE_PARAM,
    DFS_SHMEM_ERR_FREE_NONALLOCED,
    DFS_SHMEM_ERR_FREE_REMOVE_NEXT,
    DFS_SHMEM_ERR_FREE_REMOVE_PREV,
    DFS_SHMEM_ERR_END
};

struct storage
{
    queue_t       order_entry;
    queue_t       free_entry;
    unsigned int  alloc;
    size_t        size;
    size_t        act_size;
    queue_t      *free_list_head;
};

struct free_node
{
    queue_t      available_entry;
    queue_t      free_list_head; 
    unsigned int index;
};

typedef struct dfs_shmem_stat_s 
{
    size_t used_size; // not include system and storage size
    size_t reqs_size;
    size_t st_count;
    size_t st_size;
    size_t total_size;
    size_t system_size;
    size_t failed;
    size_t split_failed;
    size_t split;
} dfs_shmem_stat_t;

typedef struct dfs_shmem_s 
{
    queue_t           order;  // all block list include free and alloced
    struct free_node *free;   // free array from 0 to free_len - 1
    size_t            free_len; // length of free array
    queue_t           available; // the list of non-empty free_list
    size_t            max_available_index; //the id of last element in available
    size_t            min_size; // the size of free[0]
    size_t            max_size; // the size of free[free_len - 1]
    size_t            factor;  // increament step
    size_t            split_threshold; // threshold size for split default to min_size
    int               level_type;// increament type                    
    dfs_shmem_stat_t  shmem_stat;// stat for shmem
} dfs_shmem_t;

dfs_shmem_t* dfs_shmem_create(size_t size, size_t min_size, 
	size_t max_size, int level_type, size_t factor, unsigned int *shmem_errno);
int dfs_shmem_release(dfs_shmem_t **shm, unsigned int *shmem_errno);   
inline size_t dfs_shmem_set_alignment_size(size_t size);
inline size_t dfs_shmem_get_alignment_size();
void * dfs_shmem_alloc(dfs_shmem_t *shm, size_t size, 
	unsigned int *shmem_errno);
void * dfs_shmem_calloc(dfs_shmem_t *shm, size_t size, 
	unsigned int *shmem_errno);
void dfs_shmem_free(dfs_shmem_t *shm, void *addr, unsigned int *shmem_errno);
void * dfs_shmem_split_alloc(dfs_shmem_t *shm, size_t *act_size,
    size_t minsize, unsigned int *shmem_errno);
void       dfs_shmem_dump(dfs_shmem_t *shm, FILE *out);
size_t     dfs_shmem_get_used_size(dfs_shmem_t *shm);
size_t     dfs_shmem_get_total_size(dfs_shmem_t *shm);
size_t     dfs_shmem_get_system_size(dfs_shmem_t *shm);
inline int dfs_shmem_set_split_threshold(dfs_shmem_t *shm, size_t size);
const char * dfs_shmem_strerror(unsigned int shmem_errno);
int dfs_shmem_get_stat(dfs_shmem_t *shmem, dfs_shmem_stat_t *stat);
uchar_t *dfs_shm_strdup(dfs_shmem_t *shm, string_t *str);

#endif

