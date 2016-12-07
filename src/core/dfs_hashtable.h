#ifndef DFS_HASHTABLE_H
#define DFS_HASHTABLE_H

#include "dfs_lock.h"
#include "dfs_queue.h"
#include "dfs_mem_allocator.h"

#define  DFS_HASHTABLE_DEFAULT_SIZE       7951
#define  DFS_HASHTABLE_STORE_DEFAULT_SIZE 16777217

typedef void    DFS_HASHTABLE_FREE(void *);
typedef int     DFS_HASHTABLE_CMP(const void *, const void *, size_t);
typedef size_t  DFS_HASHTABLE_HASH(const void *, size_t, size_t);

enum 
{
    DFS_HASHTABLE_COLL_LIST,
    DFS_HASHTABLE_COLL_NEXT
};

#define DFS_HASHTABLE_ERROR    -1
#define DFS_HASHTABLE_OK        0

#define DFS_HASHTABLE_FALSE     0
#define DFS_HASHTABLE_TRUE      1

typedef struct dfs_hashtable_link_s dfs_hashtable_link_t;

struct dfs_hashtable_link_s 
{
    void                 *key;
    size_t                len;
    dfs_hashtable_link_t *next;
};

typedef struct dfs_hashtable_errno_s 
{
    unsigned int     hash_errno;
    unsigned int     allocator_errno;
    dfs_lock_errno_t lock_errno;
} dfs_hashtable_errno_t;

typedef struct dfs_hashtable_s 
{
    dfs_hashtable_link_t **buckets;
    DFS_HASHTABLE_CMP     *cmp;         // compare function
    DFS_HASHTABLE_HASH    *hash;        // hash function
    dfs_mem_allocator_t   *allocator;   // create on shmem
    size_t                 size;        // bucket number
    int                    coll;        // collection algrithm
    int                    count;       // total element that inserted to hashtable
} dfs_hashtable_t;

#define dfs_hashtable_link_make(str) {(void*)(str), sizeof((str)) - 1, NULL,NULL}
#define dfs_hashtable_link_null      {NULL, 0, NULL,NULL}

DFS_HASHTABLE_HASH dfs_hashtable_hash_hash4;
DFS_HASHTABLE_HASH dfs_hashtable_hash_key8;
DFS_HASHTABLE_HASH dfs_hashtable_hash_low;

dfs_hashtable_t * dfs_hashtable_create(DFS_HASHTABLE_CMP *cmp_func, 
	size_t hash_sz, DFS_HASHTABLE_HASH *hash_func, dfs_mem_allocator_t *allocator);
int dfs_hashtable_init(dfs_hashtable_t *ht, DFS_HASHTABLE_CMP *cmp_func,
    size_t hash_sz, DFS_HASHTABLE_HASH *hash_func, dfs_mem_allocator_t *);
int   dfs_hashtable_empty(dfs_hashtable_t *ht);
int   dfs_hashtable_join(dfs_hashtable_t *, dfs_hashtable_link_t *);
int   dfs_hashtable_remove_link(dfs_hashtable_t *, dfs_hashtable_link_t *);
void *dfs_hashtable_lookup(dfs_hashtable_t *, const void *, size_t len);
void  dfs_hashtable_free_memory(dfs_hashtable_t *);
void dfs_hashtable_free_items(dfs_hashtable_t *ht,
    void (*free_object_func)(void*), void*);
dfs_hashtable_link_t *dfs_hashtable_get_bucket(dfs_hashtable_t *, uint32_t);

#endif

