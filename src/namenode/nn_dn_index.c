#include <string>
#include "nn_dn_index.h"
#include "dfs_math.h"
#include "dfs_memory.h"
#include "fs_permission.h"
#include "nn_thread.h"
#include "nn_conf.h"
#include "nn_net_response_handler.h"
#include "nn_blk_index.h"

#define DN_NUM_IN_CLUSTER 5120
#define SEC2MSEC(X) ((X) * 1000)

extern _xvolatile rb_msec_t dfs_current_msec;

static dn_cache_mgmt_t *g_dcm = NULL;
static queue_t          g_dn_q;
static int              g_dn_n = 0;

static dn_cache_mgmt_t *dn_cache_mgmt_new_init(conf_server_t *conf);
static dn_cache_mgmt_t *dn_cache_mgmt_create(size_t index_num);
static int dn_mem_mgmt_create(dn_cache_mem_t *mem_mgmt, 
	size_t index_num);
static struct mem_mblks *dn_mblks_create(dn_cache_mem_t *mem_mgmt, 
	size_t count);
static void *allocator_malloc(void *priv, size_t mem_size);
static void allocator_free(void *priv, void *mem_addr);
static void dn_mem_mgmt_destroy(dn_cache_mem_t *mem_mgmt);
static int dn_cache_mgmt_timer_new(dn_cache_mgmt_t *dcm, 
	conf_server_t *conf);
static int dn_hash_keycmp(const void *arg1, const void *arg2, 
	size_t size);
static void dn_cache_mgmt_release(dn_cache_mgmt_t *dcm);
static void dn_timer_destroy(void *args);
static void dn_store_destroy(dn_store_t *dns);
static dn_store_t *get_dn_store_obj(uchar_t *key);
static dn_timer_t *dn_timer_create(dn_store_t *dns);
static void dn_timeout_handler(event_t *ev);
static void dn_timer_update(dn_store_t *dns);
	
int nn_dn_index_worker_init(cycle_t *cycle)
{
    conf_server_t *conf = (conf_server_t *)cycle->sconf;

	g_dcm = dn_cache_mgmt_new_init(conf);
    if (!g_dcm) 
	{
        return DFS_ERROR;
    }

	if (dn_cache_mgmt_timer_new(g_dcm, conf) != DFS_OK) 
	{
        return DFS_ERROR;
    }

	queue_init(&g_dn_q);
	g_dn_n = 0;
	
    return DFS_OK;
}

int nn_dn_index_worker_release(cycle_t *cycle)
{
    dn_cache_mgmt_release(g_dcm);
	g_dcm = NULL;
	g_dn_n = 0;

    return DFS_OK;
}

static dn_cache_mgmt_t *dn_cache_mgmt_new_init(conf_server_t *conf)
{
    size_t index_num = dfs_math_find_prime(DN_NUM_IN_CLUSTER);

    dn_cache_mgmt_t *dcm = dn_cache_mgmt_create(index_num);
    if (!dcm) 
	{
        return NULL;
    }

    pthread_rwlock_init(&dcm->cache_rwlock, NULL);

    return dcm;
}

static dn_cache_mgmt_t *dn_cache_mgmt_create(size_t index_num)
{
    dn_cache_mgmt_t *dcm = (dn_cache_mgmt_t *)memory_alloc(sizeof(*dcm));
    if (!dcm) 
	{
        goto err_out;
    }
    
    if (dn_mem_mgmt_create(&dcm->mem_mgmt, index_num) != DFS_OK) 
	{
        goto err_mem_mgmt;
    }

    dcm->dn_htable = dfs_hashtable_create(dn_hash_keycmp, index_num, 
		dfs_hashtable_hash_low, dcm->mem_mgmt.allocator);
    if (!dcm->dn_htable) 
	{
        goto err_htable;
    }

    return dcm;

err_htable:
    dn_mem_mgmt_destroy(&dcm->mem_mgmt);
	
err_mem_mgmt:
    memory_free(dcm, sizeof(*dcm));

err_out:
    return NULL;
}

static int dn_mem_mgmt_create(dn_cache_mem_t *mem_mgmt, 
	size_t index_num)
{
    assert(mem_mgmt);

    size_t mem_size = DN_POOL_SIZE(index_num);

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

    mem_mgmt->free_mblks = dn_mblks_create(mem_mgmt, index_num);
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

static struct mem_mblks *dn_mblks_create(dn_cache_mem_t *mem_mgmt, 
	size_t count)
{
    assert(mem_mgmt);
	
    mem_mblks_param_t mblk_param;
    mblk_param.mem_alloc = allocator_malloc;
    mblk_param.mem_free = allocator_free;
    mblk_param.priv = mem_mgmt->allocator;

    return mem_mblks_new(dn_store_t, count, &mblk_param);
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

static void dn_mem_mgmt_destroy(dn_cache_mem_t *mem_mgmt)
{
    mem_mblks_destroy(mem_mgmt->free_mblks); 
    dfs_mem_allocator_delete(mem_mgmt->allocator);
    memory_free(mem_mgmt->mem, mem_mgmt->mem_size);
}

static int dn_cache_mgmt_timer_new(dn_cache_mgmt_t *dcm, 
	conf_server_t *conf)
{
    assert(dcm);

    dcm->dn_timer_htable = dfs_hashtable_create(dn_hash_keycmp, 
		DN_NUM_IN_CLUSTER, dfs_hashtable_hash_key8, NULL);
    if (!dcm->dn_timer_htable)
	{
        return DFS_ERROR;
    }

    dcm->timeout = SEC2MSEC(conf->dn_timeout);
    pthread_rwlock_init(&dcm->timer_rwlock, NULL);

    return DFS_OK;
}

static int dn_hash_keycmp(const void *arg1, const void *arg2, 
	size_t size)
{
    return string_strncmp(arg1, arg2, size);
}

static void dn_cache_mgmt_release(dn_cache_mgmt_t *dcm)
{
    assert(dcm);

    pthread_rwlock_wrlock(&dcm->timer_rwlock);
    dfs_hashtable_free_items(dcm->dn_timer_htable, dn_timer_destroy, NULL);
    pthread_rwlock_unlock(&dcm->timer_rwlock);

	pthread_rwlock_destroy(&dcm->cache_rwlock);
	pthread_rwlock_destroy(&dcm->timer_rwlock);

    dn_mem_mgmt_destroy(&dcm->mem_mgmt);
    memory_free(dcm, sizeof(*dcm));
}

static void dn_timer_destroy(void *args)
{
    assert(args);
	
    dn_timer_t *dt = (dn_timer_t *)args;
    memory_free(dt, sizeof(*dt));
}

static void dn_store_destroy(dn_store_t *dns)
{
    assert(dns);
	
	mem_put(dns);
}

int nn_dn_register(task_t *task)
{
    task_queue_node_t *node = queue_data(task, task_queue_node_t, tk);

	dn_store_t *dns = get_dn_store_obj((uchar_t *)task->key);
	if (dns) 
	{
	    dn_timer_update(dns);
	    
		goto out;
	}

	pthread_rwlock_wrlock(&g_dcm->cache_rwlock);

	dns = (dn_store_t *)mem_get0(g_dcm->mem_mgmt.free_mblks);
	if (!dns)
	{
	    dfs_log_error(dfs_cycle->error_log, DFS_LOG_ALERT, 0, "mem_get0 err");
	}

	queue_init(&dns->me);
	queue_init(&dns->blk);
	queue_init(&dns->del_blk);

	dns->del_blk_num = 0;

	strcpy(dns->dni.id, task->key);

	dns->ln.key = dns->dni.id;
    dns->ln.len = string_strlen(dns->dni.id);
    dns->ln.next = NULL;

	dfs_hashtable_join(g_dcm->dn_htable, &dns->ln);

	queue_insert_tail(&g_dn_q, &dns->me);
	g_dn_n++;

	pthread_rwlock_unlock(&g_dcm->cache_rwlock);

	dn_timer_create(dns);

out:
	dfs_log_error(dfs_cycle->error_log, DFS_LOG_INFO, 0, 
		"datanode %s register", dns->dni.id);
	
    task->ret = DFS_OK;

	task->data_len = sizeof(int64_t);
	task->data = malloc(task->data_len);
	if (NULL == task->data) 
	{
        dfs_log_error(dfs_cycle->error_log, DFS_LOG_ALERT, 0, "malloc err");
	}

	*(int64_t *)task->data = dfs_cycle->namespace_id;

    return write_back(node);
}

static dn_store_t *get_dn_store_obj(uchar_t *key)
{
    pthread_rwlock_rdlock(&g_dcm->cache_rwlock);

	dn_store_t *dns = (dn_store_t *)dfs_hashtable_lookup(g_dcm->dn_htable, 
		(void *)key, string_strlen(key));

	pthread_rwlock_unlock(&g_dcm->cache_rwlock);
	
    return dns;
}

int nn_dn_heartbeat(task_t *task)
{
    dfs_log_error(dfs_cycle->error_log, DFS_LOG_INFO, 
			0, "Got datanode %s heartbeat", task->key);
	
    task_queue_node_t *node = queue_data(task, task_queue_node_t, tk);

	dn_store_t *dns = get_dn_store_obj((uchar_t *)task->key);
	if (dns) 
	{
	    dn_timer_update(dns);

		if (dns->del_blk_num > 0) 
		{
            int del_blk_num = dns->del_blk_num > DELETING_BLK_FOR_ONCE 
				? DELETING_BLK_FOR_ONCE : dns->del_blk_num;

			task->data_len = del_blk_num * sizeof(uint64_t);
			task->data = malloc(task->data_len);
			if (NULL == task->data) 
			{
                dfs_log_error(dfs_cycle->error_log, DFS_LOG_ALERT, 0, 
					"malloc err");
			}

			void *pData = task->data;

			queue_t *cur = NULL;
			del_blk_t *dblk = NULL;

			pthread_rwlock_rdlock(&g_dcm->cache_rwlock);

			while (del_blk_num > 0) 
			{
				cur = queue_head(&dns->del_blk);
				queue_remove(cur);
				dns->del_blk_num--;
				
				dblk = queue_data(cur, del_blk_t, me);
				
				memcpy(pData, &dblk->id, sizeof(dblk->id));
				pData += sizeof(dblk->id);

				free(dblk);
				dblk = NULL;

				del_blk_num--;
			}
			
			pthread_rwlock_unlock(&g_dcm->cache_rwlock);
		}
		
		task->ret = DFS_OK;
	}
    else 
	{
	    dfs_log_error(dfs_cycle->error_log, DFS_LOG_FATAL, 
			0, "datanode %s haven't registered yet", task->key);
		
        task->ret = DFS_ERROR;
	}

    return write_back(node);
}

static dn_timer_t *dn_timer_create(dn_store_t *dns)
{    
    dn_timer_t *dt = (dn_timer_t *)memory_calloc(sizeof(dn_timer_t));
    if (!dt) 
	{
        dfs_log_error(dfs_cycle->error_log, DFS_LOG_FATAL, 
			errno, "dn_timer_create: memory error! ");
		
        return NULL;
    }

    dt->ln.key = dns->dni.id;
    dt->ln.len = string_strlen(dns->dni.id);
    dt->ln.next = NULL;
	
    dt->ev.data = (void *)dt;
	dt->ev.handler = dn_timeout_handler;

	dt->thread = get_local_thread();
	dt->dns = dns;

    pthread_rwlock_wrlock(&g_dcm->timer_rwlock);
	
	dfs_hashtable_join(g_dcm->dn_timer_htable, &dt->ln);

	pthread_rwlock_unlock(&g_dcm->timer_rwlock);

	event_timer_add(&dt->thread->event_timer, &dt->ev, g_dcm->timeout);

    return dt;
}

static void dn_timeout_handler(event_t *ev)
{
    assert(ev);

    dn_timer_t *dt = (dn_timer_t *)ev->data;
	dn_store_t *dns = (dn_store_t *)dt->dns;

	dfs_log_error(dfs_cycle->error_log, DFS_LOG_INFO, 0, 
		"datanode %s is dead", dns->dni.id);

	pthread_rwlock_wrlock(&g_dcm->timer_rwlock);
	
	dfs_hashtable_remove_link(g_dcm->dn_timer_htable, &dt->ln);
	
	pthread_rwlock_unlock(&g_dcm->timer_rwlock);

	pthread_rwlock_wrlock(&g_dcm->cache_rwlock);
	
	dfs_hashtable_remove_link(g_dcm->dn_htable, &dns->ln);
	queue_remove(&dns->me);
	g_dn_n--;
	
	pthread_rwlock_unlock(&g_dcm->cache_rwlock);

    dn_store_destroy(dns);
	dn_timer_destroy(dt);
}

static void dn_timer_update(dn_store_t *dns)
{
    pthread_rwlock_wrlock(&g_dcm->timer_rwlock);
	
	dn_timer_t *dt = (dn_timer_t *)dfs_hashtable_lookup(g_dcm->dn_timer_htable, 
		(void *)dns->dni.id, string_strlen(dns->dni.id));

	event_timer_add(&dt->thread->event_timer, &dt->ev, g_dcm->timeout);
	
	pthread_rwlock_unlock(&g_dcm->timer_rwlock);
}

int nn_dn_recv_blk_report(task_t *task)
{
	int          rs = DFS_OK;
	dn_store_t  *dns = NULL;
	blk_store_t *blk = NULL;

	report_blk_info_t rbi;
	memset(&rbi, 0x00, sizeof(report_blk_info_t));
	memcpy(&rbi, task->data, sizeof(report_blk_info_t));

	task->data = NULL;
	task->data_len = 0;

	task_queue_node_t *node = queue_data(task, task_queue_node_t, tk);

	dns = get_dn_store_obj((uchar_t*)task->key);
	if (!dns) 
	{
        dfs_log_error(dfs_cycle->error_log, DFS_LOG_FATAL, 0, 
			"blk %d is from dead or unregistered node %s", 
			rbi.blk_id, rbi.dn_ip);

		rs = DFS_ERROR;

		goto out;
	}

	blk = add_block(rbi.blk_id, rbi.blk_sz, rbi.dn_ip);
    if (!blk) 
	{
        rs = DFS_ERROR;
	}

    pthread_rwlock_wrlock(&g_dcm->cache_rwlock);
	
	queue_insert_tail(&dns->blk, &blk->dn_me);

	pthread_rwlock_unlock(&g_dcm->cache_rwlock);
	
out:
	task->ret = rs;

    return write_back(node);
}

int nn_dn_del_blk_report(task_t *task)
{
    return DFS_OK;
}

int nn_dn_blk_report(task_t *task)
{
	int          rs = DFS_OK;
	dn_store_t  *dns = NULL;
	blk_store_t *blk = NULL;

	report_blk_info_t rbi;
	memset(&rbi, 0x00, sizeof(report_blk_info_t));
	memcpy(&rbi, task->data, sizeof(report_blk_info_t));

	task->data = NULL;
	task->data_len = 0;

	task_queue_node_t *node = queue_data(task, task_queue_node_t, tk);

	dns = get_dn_store_obj((uchar_t*)task->key);
	if (!dns) 
	{
        dfs_log_error(dfs_cycle->error_log, DFS_LOG_FATAL, 0, 
			"blk %d is from dead or unregistered node %s", 
			rbi.blk_id, rbi.dn_ip);

		rs = DFS_ERROR;

		goto out;
	}

	blk = get_blk_store_obj(rbi.blk_id);
	if (blk) 
	{
	    // check diff
	    
        rs = DFS_OK;

		goto out;
	}

	blk = add_block(rbi.blk_id, rbi.blk_sz, rbi.dn_ip);
    if (!blk) 
	{
        rs = DFS_ERROR;
	}

    pthread_rwlock_wrlock(&g_dcm->cache_rwlock);
	
	queue_insert_tail(&dns->blk, &blk->dn_me);

	pthread_rwlock_unlock(&g_dcm->cache_rwlock);
	
out:
	task->ret = rs;

    return write_back(node);
}

int generate_dns(short blk_rep, create_resp_info_t *resp_info)
{
    queue_t    *cur = NULL;
	dn_store_t *dns = NULL;
	
    pthread_rwlock_wrlock(&g_dcm->cache_rwlock);
	
	if (0 == g_dn_n) 
	{
        pthread_rwlock_unlock(&g_dcm->cache_rwlock);

		dfs_log_error(dfs_cycle->error_log, DFS_LOG_FATAL, 0, 
			"no avalable datanode");

		return DFS_ERROR;
	}

	cur = queue_head(&g_dn_q);
	dns = queue_data(cur, dn_store_t, me);
	
	resp_info->dn_num = 1;
	strcpy(resp_info->dn_ips[0], dns->dni.id);
	strcpy(resp_info->dn_ips[1], "");
	strcpy(resp_info->dn_ips[2], "");

	pthread_rwlock_unlock(&g_dcm->cache_rwlock);
	
    return DFS_OK;
}

int notify_dn_2_delete_blk(long blk_id, char dn_ip[32])
{
    dn_store_t *dns = get_dn_store_obj((uchar_t *)dn_ip);
	if (!dns) 
	{
	    return DFS_ERROR;
	}

	del_blk_t *dblk = (del_blk_t *)malloc(sizeof(del_blk_t));
	if (!dblk) 
	{
        dfs_log_error(dfs_cycle->error_log, DFS_LOG_FATAL, 0, 
			"malloc err");

		return DFS_ERROR;
	}

	queue_init(&dblk->me);
	dblk->id = blk_id;
	
    pthread_rwlock_wrlock(&g_dcm->cache_rwlock);

	queue_insert_tail(&dns->del_blk, &dblk->me);
	
	dns->del_blk_num++;

	pthread_rwlock_unlock(&g_dcm->cache_rwlock);
	
    return DFS_OK;
}

