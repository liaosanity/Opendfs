#ifndef NN_BLK_INDEX_H
#define NN_BLK_INDEX_H

#include "dfs_queue.h"
#include "dfs_hashtable.h"
#include "nn_cycle.h"

#define PATH_LEN 256
#define SUBDIR_LEN 64

#define HASH_BUF_PER_SZ sizeof(void *) 
#define DFS_ALIGNMENT sizeof(uint64_t)
#define BLK_STORE_SIZE (size_t)(dfs_align_ptr(sizeof(blk_store_t), DFS_ALIGNMENT))
#define BLK_STORE_BUF_PER_SZ (BLK_STORE_SIZE + sizeof(void *))

#define BLK_POOL_REMAIN_MEM (10 * 1024)

#define BLK_HASH_BUF(count)  (count * HASH_BUF_PER_SZ)
#define BLK_STORE_BUF(count) (count * BLK_STORE_BUF_PER_SZ)

#define BLK_POOL_SIZE(count) (BLK_HASH_BUF(count) \
        + BLK_STORE_BUF(count) + BLK_POOL_REMAIN_MEM) 

typedef struct blk_store_s
{
    dfs_hashtable_link_t ln;
    queue_t              fi_me;
	queue_t              dn_me;
    long                 id;
	uint64_t             size;
	char                 dn_ip[32];
} blk_store_t;

typedef struct blk_cache_mem_s 
{
    void                *mem;
    size_t               mem_size;
    dfs_mem_allocator_t *allocator;
    struct mem_mblks    *free_mblks;
} blk_cache_mem_t;

typedef struct blk_cache_mgmt_s 
{
    dfs_hashtable_t  *blk_htable;
    pthread_rwlock_t  cache_rwlock;
    blk_cache_mem_t   mem_mgmt;
} blk_cache_mgmt_t;

int nn_blk_index_worker_init(cycle_t *cycle);
int nn_blk_index_worker_release(cycle_t *cycle);

blk_store_t *get_blk_store_obj(long id);
int block_object_del(long id);

blk_store_t *add_block(long blk_id, long blk_sz, char dn_ip[32]);

uint64_t generate_uid();

int notify_dn_2_delete_blk(long blk_id, char dn_ip[32]);

#endif

