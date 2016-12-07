#ifndef NN_DN_INDEX_H
#define NN_DN_INDEX_H

#include "dfs_hashtable.h"
#include "dfs_mem_allocator.h"
#include "dfs_mblks.h"
#include "dfs_commpool.h"
#include "dfs_task.h"
#include "dfs_event.h"
#include "nn_cycle.h"
#include "nn_thread.h"

/*
#define DN_UNKNOWN      0    
#define DN_TRANSFER     1 
#define DN_INVALIDATE   2 
#define DN_SHUTDOWN     3   
#define DN_REGISTER     4   
#define DN_FINALIZE     5   
#define DN_RECOVERBLOCK 6
*/

#define ID_LEN 32

#define HASH_BUF_PER_SZ sizeof(void *) 
#define DFS_ALIGNMENT sizeof(uint64_t)
#define DN_STORE_SIZE (size_t)(dfs_align_ptr(sizeof(dn_store_t), DFS_ALIGNMENT))
#define DN_STORE_BUF_PER_SZ (DN_STORE_SIZE + sizeof(void *))

#define DN_POOL_REMAIN_MEM (10 * 1024)

#define DN_HASH_BUF(dn_count)  (dn_count * HASH_BUF_PER_SZ)
#define DN_STORE_BUF(dn_count) (dn_count * DN_STORE_BUF_PER_SZ)

#define DN_POOL_SIZE(dn_count) (DN_HASH_BUF(dn_count) \
        + DN_STORE_BUF(dn_count) + DN_POOL_REMAIN_MEM) 

#define DELETING_BLK_FOR_ONCE 5

typedef struct del_blk_s
{
    queue_t  me;
	uint64_t id;
} del_blk_t;

typedef struct dn_info_s
{
	char     id[ID_LEN];
	uint64_t capacity;
	uint64_t dfs_used;
	uint32_t remaining;
	uint64_t namespace_used;
	uint64_t last_update;
	int      active_conn;
} dn_info_t;

typedef struct dn_store_s 
{
	dfs_hashtable_link_t ln;
	queue_t 	         me;
	queue_t              blk;
	queue_t              del_blk;
	uint64_t             del_blk_num;
	dn_info_t            dni;
} dn_store_t;

typedef struct dn_timer_s {
    dfs_hashtable_link_t  ln;
	dfs_thread_t	     *thread;
	event_t 		      ev;
	dn_store_t           *dns;
} dn_timer_t;
        
typedef struct dn_cache_mem_s 
{
    void                *mem;
    size_t               mem_size;
    dfs_mem_allocator_t *allocator;
    struct mem_mblks    *free_mblks;
} dn_cache_mem_t;

typedef struct dn_cache_mgmt_s 
{
    dfs_hashtable_t  *dn_htable;
    pthread_rwlock_t  cache_rwlock;
    dn_cache_mem_t    mem_mgmt;
    dfs_hashtable_t  *dn_timer_htable;
    pthread_rwlock_t  timer_rwlock;
	int               timeout; // MSec
} dn_cache_mgmt_t;

int nn_dn_index_worker_init(cycle_t *cycle);
int nn_dn_index_worker_release(cycle_t *cycle);

int nn_dn_register(task_t *task);
int nn_dn_heartbeat(task_t *task);
int nn_dn_recv_blk_report(task_t *task);
int nn_dn_del_blk_report(task_t *task);
int nn_dn_blk_report(task_t *task);

int generate_dns(short blk_rep, create_resp_info_t *resp_info);
#endif

