#ifndef DN_DATA_STORAGE
#define DN_DATA_STORAGE

#include "dfs_queue.h"
#include "dn_cycle.h"
#include "dn_thread.h"
#include "dn_request.h"
#include "cfs_fio.h"

#define PATH_LEN 256
#define SUBDIR_LEN 64

#define HASH_BUF_PER_SZ sizeof(void *) 
#define DFS_ALIGNMENT sizeof(uint64_t)
#define BLK_STORE_SIZE (size_t)(dfs_align_ptr(sizeof(block_info_t), DFS_ALIGNMENT))
#define BLK_STORE_BUF_PER_SZ (BLK_STORE_SIZE + sizeof(void *))

#define BLK_POOL_REMAIN_MEM (10 * 1024)

#define BLK_HASH_BUF(count)  (count * HASH_BUF_PER_SZ)
#define BLK_STORE_BUF(count) (count * BLK_STORE_BUF_PER_SZ)

#define BLK_POOL_SIZE(count) (BLK_HASH_BUF(count) \
        + BLK_STORE_BUF(count) + BLK_POOL_REMAIN_MEM) 

typedef struct storage_dir_s 
{
    queue_t me;
    int     id;
	char    current[PATH_LEN];
} storage_dir_t;

typedef struct block_info_s
{
    dfs_hashtable_link_t ln;
    queue_t              me;
    long                 id;
	long                 size;
	char                 path[PATH_LEN];
} block_info_t;

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

int dn_data_storage_master_init(cycle_t *cycle);
int dn_data_storage_worker_init(cycle_t *cycle);
int dn_data_storage_worker_release(cycle_t *cycle);
int dn_data_storage_thread_init(dfs_thread_t *thread);

int setup_ns_storage(dfs_thread_t *thread);

block_info_t *block_object_get(long id);
int block_object_add(char *path, long ns_id, long blk_id);
int block_object_del(long blk_id);
int block_read(dn_request_t *r, file_io_t *fio);

void io_lock(volatile uint64_t *lock);
uint64_t io_unlock(volatile uint64_t *lock);
int io_lock_zero(volatile uint64_t *lock);

int get_block_temp_path(dn_request_t *r);
int write_block_done(dn_request_t *r);

void *blk_scanner_start(void *arg);

#endif

