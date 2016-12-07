#include "nn_blk_index.h"
#include "dfs_types.h"
#include "dfs_math.h"
#include "dfs_memory.h"
#include "dfs_commpool.h"
#include "dfs_mblks.h"
#include "nn_time.h"

#define BLK_NUM_IN_DN 100000

typedef struct my_uid_s 
{
    uint32_t sec;
    uint32_t seq;
} my_uid_t;

typedef struct uid_context_s 
{
    uint32_t          sequence;
    dfs_atomic_lock_t seq_lock;
} uid_context_t;

static uid_context_t g_uid_ctx;

static blk_cache_mgmt_t *g_nn_bcm = NULL;

static blk_cache_mgmt_t *blk_cache_mgmt_new_init();
static blk_cache_mgmt_t *blk_cache_mgmt_create(size_t index_num);
static int blk_mem_mgmt_create(blk_cache_mem_t *mem_mgmt, 
	size_t index_num);
static struct mem_mblks *blk_mblks_create(blk_cache_mem_t *mem_mgmt, 
	size_t count);
static void *allocator_malloc(void *priv, size_t mem_size);
static void allocator_free(void *priv, void *mem_addr);
static void blk_mem_mgmt_destroy(blk_cache_mem_t *mem_mgmt);
static void blk_cache_mgmt_release(blk_cache_mgmt_t *bcm);
static int uint64_cmp(const void *s1, const void *s2, size_t sz);
static size_t req_hash(const void *data, size_t data_size, 
	size_t hashtable_size);

int nn_blk_index_worker_init(cycle_t *cycle)
{
	g_nn_bcm = blk_cache_mgmt_new_init();
    if (!g_nn_bcm) 
	{
        return DFS_ERROR;
    }

	dfs_atomic_lock_init(&g_uid_ctx.seq_lock);
	
    return DFS_OK;
}

int nn_blk_index_worker_release(cycle_t *cycle)
{
    blk_cache_mgmt_release(g_nn_bcm);
	g_nn_bcm = NULL;
	
    return DFS_OK;
}

static blk_cache_mgmt_t *blk_cache_mgmt_new_init()
{
    size_t index_num = dfs_math_find_prime(BLK_NUM_IN_DN);

    blk_cache_mgmt_t *bcm = blk_cache_mgmt_create(index_num);
    if (!bcm) 
	{
        return NULL;
    }

    pthread_rwlock_init(&bcm->cache_rwlock, NULL);

    return bcm;
}

static blk_cache_mgmt_t *blk_cache_mgmt_create(size_t index_num)
{
    blk_cache_mgmt_t *bcm = (blk_cache_mgmt_t *)memory_alloc(sizeof(*bcm));
    if (!bcm) 
	{
        goto err_out;
    }
    
    if (blk_mem_mgmt_create(&bcm->mem_mgmt, index_num) != DFS_OK) 
	{
        goto err_mem_mgmt;
    }

    bcm->blk_htable = dfs_hashtable_create(uint64_cmp, index_num, 
		req_hash, bcm->mem_mgmt.allocator);
    if (!bcm->blk_htable) 
	{
        goto err_htable;
    }

    return bcm;

err_htable:
    blk_mem_mgmt_destroy(&bcm->mem_mgmt);
	
err_mem_mgmt:
    memory_free(bcm, sizeof(*bcm));

err_out:
    return NULL;
}

static int blk_mem_mgmt_create(blk_cache_mem_t *mem_mgmt, 
	size_t index_num)
{
    assert(mem_mgmt);

    size_t mem_size = BLK_POOL_SIZE(index_num);

    mem_mgmt->mem = memory_calloc(mem_size);
    if (!mem_mgmt->mem) 
	{
        goto err_mem;
    }

    mem_mgmt->mem_size = mem_size;

	mpool_mgmt_param_t param;
    param.mem_addr = mem_mgmt->mem;
    param.mem_size = mem_size;

    mem_mgmt->allocator = dfs_mem_allocator_new_init(
		DFS_MEM_ALLOCATOR_TYPE_COMMPOOL, &param);
    if (!mem_mgmt->allocator) 
	{
        goto err_allocator;
    }

    mem_mgmt->free_mblks = blk_mblks_create(mem_mgmt, index_num);
    if (!mem_mgmt->free_mblks) 
	{
        goto err_mblks;
    }

    return DFS_OK;

err_mblks:
    dfs_mem_allocator_delete(mem_mgmt->allocator);
	
err_allocator:
    memory_free(mem_mgmt->mem, mem_mgmt->mem_size);
	
err_mem:
    return DFS_ERROR;
}

static struct mem_mblks *blk_mblks_create(blk_cache_mem_t *mem_mgmt, 
	size_t count)
{
    assert(mem_mgmt);
	
    mem_mblks_param_t mblk_param;
    mblk_param.mem_alloc = allocator_malloc;
    mblk_param.mem_free = allocator_free;
    mblk_param.priv = mem_mgmt->allocator;

    return mem_mblks_new(blk_store_t, count, &mblk_param);
}

static void *allocator_malloc(void *priv, size_t mem_size)
{
    if (!priv) 
	{
        return NULL;
    }

	dfs_mem_allocator_t *allocator = (dfs_mem_allocator_t *)priv;
	
    return allocator->alloc(allocator, mem_size, NULL);
}

static void allocator_free(void *priv, void *mem_addr)
{
    if (!priv || !mem_addr) 
	{
        return;
    }

    dfs_mem_allocator_t *allocator = (dfs_mem_allocator_t *)priv;
    allocator->free(allocator, mem_addr, NULL);
}

static void blk_mem_mgmt_destroy(blk_cache_mem_t *mem_mgmt)
{
    mem_mblks_destroy(mem_mgmt->free_mblks); 
    dfs_mem_allocator_delete(mem_mgmt->allocator);
    memory_free(mem_mgmt->mem, mem_mgmt->mem_size);
}

static void blk_cache_mgmt_release(blk_cache_mgmt_t *bcm)
{
    assert(bcm);

	pthread_rwlock_destroy(&bcm->cache_rwlock);

    blk_mem_mgmt_destroy(&bcm->mem_mgmt);
    memory_free(bcm, sizeof(*bcm));
}

static int uint64_cmp(const void *s1, const void *s2, size_t sz)
{
    return *(uint64_t *)s1 == *(uint64_t *)s2 ? DFS_FALSE : DFS_TRUE; 
}

static size_t req_hash(const void *data, size_t data_size, 
	size_t hashtable_size)
{
    uint64_t u = *(uint64_t *)data;
	
    return u % hashtable_size;
}

blk_store_t *get_blk_store_obj(long id)
{
    pthread_rwlock_rdlock(&g_nn_bcm->cache_rwlock);

	blk_store_t *blk = (blk_store_t *)dfs_hashtable_lookup(g_nn_bcm->blk_htable, 
		&id, sizeof(id));

	pthread_rwlock_unlock(&g_nn_bcm->cache_rwlock);
	
    return blk;
}

int block_object_del(long id)
{
    blk_store_t *blk = NULL;
	
	blk = get_blk_store_obj(id);
	if (!blk) 
	{
        return DFS_OK;
	}

	notify_dn_2_delete_blk(id, blk->dn_ip);

	queue_remove(&blk->dn_me);
	
    pthread_rwlock_wrlock(&g_nn_bcm->cache_rwlock);

    dfs_hashtable_remove_link(g_nn_bcm->blk_htable, &blk->ln);

	mem_put(blk);

	pthread_rwlock_unlock(&g_nn_bcm->cache_rwlock);
    
    return DFS_OK;
}

blk_store_t *add_block(long blk_id, long blk_sz, char dn_ip[32])
{
    blk_store_t *blk = NULL;

	pthread_rwlock_wrlock(&g_nn_bcm->cache_rwlock);

	blk = (blk_store_t *)mem_get0(g_nn_bcm->mem_mgmt.free_mblks);
	if (!blk)
	{
	    dfs_log_error(dfs_cycle->error_log, DFS_LOG_ALERT, 0, "mem_get0 err");

		return NULL;
	}

	queue_init(&blk->fi_me);
	queue_init(&blk->dn_me);
	
	blk->id = blk_id;
	blk->size = blk_sz;
	strcpy(blk->dn_ip, dn_ip);

	blk->ln.key = &blk->id;
    blk->ln.len = sizeof(blk->id);
    blk->ln.next = NULL;

	dfs_hashtable_join(g_nn_bcm->blk_htable, &blk->ln);

	pthread_rwlock_unlock(&g_nn_bcm->cache_rwlock);
	
    return blk;
}

uint64_t generate_uid()
{
    dfs_lock_errno_t  lerr;
    uint64_t          uid = 0;
    my_uid_t         *puid = NULL;

    puid = (my_uid_t *)&uid;
    puid->sec = time_curtime() / 1000;
    
    dfs_atomic_lock_on(&g_uid_ctx.seq_lock, &lerr);
    
    puid->seq = g_uid_ctx.sequence++;
    
    dfs_atomic_lock_off(&g_uid_ctx.seq_lock, &lerr);

    return uid;
}


