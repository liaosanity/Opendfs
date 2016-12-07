#include <dirent.h>
#include <string>
#include "nn_file_index.h"
#include "dfs_math.h"
#include "dfs_memory.h"
#include "nn_paxos.h"
#include "fs_permission.h"
#include "phxeditlog.pb.h"
#include "nn_thread.h"
#include "nn_conf.h"
#include "nn_net_response_handler.h"
#include "nn_blk_index.h"
#include "nn_dn_index.h"

using namespace phxpaxos;
using namespace phxeditlog;
using namespace std;

#define FINDEX_TIMER_NR 10000
#define SEC2MSEC(X) ((X) * 1000)
#define FI_CREATE_TIME_OUT (60 * 60 * 1000)

extern _xvolatile rb_msec_t dfs_current_msec;

uint64_t lastCheckpointInstanceID = 0;
extern dfs_thread_t    *paxos_thread;
static fi_cache_mgmt_t *g_fcm;
static queue_t          g_checkpoint_q;

dfs_atomic_lock_t g_fs_object_num_lock;
uint64_t          g_fs_object_num;

static fi_cache_mgmt_t *fi_cache_mgmt_new_init(conf_server_t *conf);
static fi_cache_mgmt_t *fi_cache_mgmt_create(size_t index_num);
static int fi_mem_mgmt_create(fi_cache_mem_t *mem_mgmt, 
	size_t index_num);
static struct mem_mblks *fi_mblks_create(fi_cache_mem_t *mem_mgmt, 
	size_t count);
static void *allocator_malloc(void *priv, size_t mem_size);
static void allocator_free(void *priv, void *mem_addr);
static void fi_mem_mgmt_destroy(fi_cache_mem_t *mem_mgmt);
static int fi_cache_mgmt_timer_new(fi_cache_mgmt_t *fcm, 
	conf_server_t *conf);
static int fi_hash_keycmp(const void *arg1, const void *arg2, 
	size_t size);
static void fi_cache_mgmt_release(fi_cache_mgmt_t *fcm);
static void fi_timer_destroy(void *args);
static void fi_store_destroy(fi_store_t *fis);
static void get_parent_key(uchar_t path[], uchar_t key[]);
static int update_fi_mkdir(fi_inode_t *fin);
static int update_fi_rmr(fi_inode_t *fin);
static int clear_children(queue_t *head, uint64_t num);
static int save_image();
static int save_checkpoinID();
static int read_checkpoinID();
static int mv_current();
static int copy_file(const char *src, const char *dst);
static int mv_last_checkpoint();
static int delete_dir(const char *dir);
static int update_fi_create(fi_inode_t *fin, uint64_t blk_id, void *data);
static void fi_create_timeout(event_t *ev);
static int update_fi_get_additional_blk(fi_inode_t *fin, 
	uint64_t blk_id);
static int update_fi_close(fi_inode_t *fin);
static int update_fi_rm(fi_inode_t *fin);
	
int nn_file_index_worker_init(cycle_t *cycle)
{
    conf_server_t *conf = (conf_server_t *)cycle->sconf;

	g_fcm = fi_cache_mgmt_new_init(conf);
    if (!g_fcm) 
	{
        return DFS_ERROR;
    }

	if (fi_cache_mgmt_timer_new(g_fcm, conf) != DFS_OK) 
	{
        return DFS_ERROR;
    }

    dfs_atomic_lock_init(&g_fs_object_num_lock);
	g_fs_object_num = 0;

	queue_init(&g_checkpoint_q);
	
    return DFS_OK;
}

int nn_file_index_worker_release(cycle_t *cycle)
{
    fi_cache_mgmt_release(g_fcm);
	g_fcm = NULL;

    return DFS_OK;
}

static fi_cache_mgmt_t *fi_cache_mgmt_new_init(conf_server_t *conf)
{
    size_t index_num = dfs_math_find_prime(conf->index_num);

    fi_cache_mgmt_t *fcm = fi_cache_mgmt_create(index_num);
    if (!fcm) 
	{
        return NULL;
    }

    pthread_rwlock_init(&fcm->cache_rwlock, NULL);

    return fcm;
}

static fi_cache_mgmt_t *fi_cache_mgmt_create(size_t index_num)
{
    fi_cache_mgmt_t *fcm = (fi_cache_mgmt_t *)memory_alloc(sizeof(*fcm));
    if (!fcm) 
	{
        goto err_out;
    }
    
    if (fi_mem_mgmt_create(&fcm->mem_mgmt, index_num) != DFS_OK) 
	{
        goto err_mem_mgmt;
    }

    fcm->fi_htable = dfs_hashtable_create(fi_hash_keycmp, index_num, 
		//dfs_hashtable_hash_key8, fcm->mem_mgmt.allocator);
		dfs_hashtable_hash_low, fcm->mem_mgmt.allocator);
    if (!fcm->fi_htable) 
	{
        goto err_htable;
    }

    return fcm;

err_htable:
    fi_mem_mgmt_destroy(&fcm->mem_mgmt);
	
err_mem_mgmt:
    memory_free(fcm, sizeof(*fcm));

err_out:
    return NULL;
}

static int fi_mem_mgmt_create(fi_cache_mem_t *mem_mgmt, 
	size_t index_num)
{
    assert(mem_mgmt);

    size_t mem_size = FI_POOL_SIZE(index_num);

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

    mem_mgmt->free_mblks = fi_mblks_create(mem_mgmt, index_num);
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

static struct mem_mblks *fi_mblks_create(fi_cache_mem_t *mem_mgmt, 
	size_t count)
{
    assert(mem_mgmt);
	
    mem_mblks_param_t mblk_param;
    mblk_param.mem_alloc = allocator_malloc;
    mblk_param.mem_free = allocator_free;
    mblk_param.priv = mem_mgmt->allocator;

    return mem_mblks_new(fi_store_t, count, &mblk_param);
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

static void fi_mem_mgmt_destroy(fi_cache_mem_t *mem_mgmt)
{
    mem_mblks_destroy(mem_mgmt->free_mblks); 
    dfs_mem_allocator_delete(mem_mgmt->allocator);
    memory_free(mem_mgmt->mem, mem_mgmt->mem_size);
}

static int fi_cache_mgmt_timer_new(fi_cache_mgmt_t *fcm, 
	conf_server_t *conf)
{
    assert(fcm);

    fcm->fi_timer_htable = dfs_hashtable_create(fi_hash_keycmp, 
		FINDEX_TIMER_NR, dfs_hashtable_hash_key8, NULL);
    if (!fcm->fi_timer_htable)
	{
        return DFS_ERROR;
    }

    //fcm->timer_delay = SEC2MSEC(conf->fi_state_lease);
    pthread_rwlock_init(&fcm->timer_rwlock, NULL);

    return DFS_OK;
}

static int fi_hash_keycmp(const void *arg1, const void *arg2, 
	size_t size)
{
    //return memcmp(arg1, arg2, size);
    return string_strncmp(arg1, arg2, size);
}

static void fi_cache_mgmt_release(fi_cache_mgmt_t *fcm)
{
    assert(fcm);

    pthread_rwlock_wrlock(&fcm->timer_rwlock);
    dfs_hashtable_free_items(fcm->fi_timer_htable, fi_timer_destroy, NULL);
    pthread_rwlock_unlock(&fcm->timer_rwlock);

	pthread_rwlock_destroy(&fcm->cache_rwlock);
	pthread_rwlock_destroy(&fcm->timer_rwlock);

    fi_mem_mgmt_destroy(&fcm->mem_mgmt);
    memory_free(fcm, sizeof(*fcm));
}

static void fi_timer_destroy(void *args)
{
    assert(args);
	
    //fi_timer_t *ft = (fi_timer_t *)args;
    //memory_free(ft, sizeof(*ft));
}

static void fi_store_destroy(fi_store_t *fis)
{
    assert(fis);
	
	mem_put(fis);
}

fi_store_t *get_store_obj(uchar_t *key)
{
    pthread_rwlock_rdlock(&g_fcm->cache_rwlock);

	fi_store_t *fi = (fi_store_t *)dfs_hashtable_lookup(g_fcm->fi_htable, 
		(void *)key, string_strlen(key));

	pthread_rwlock_unlock(&g_fcm->cache_rwlock);
	
    return fi;
}

void get_store_path(uchar_t *key, uchar_t *path)
{
	string_t src;
    string_set(src, key);

    string_t dst;
    string_set(dst, path);

    string_base64_decode(&dst, &src);
}

int get_path_names(uchar_t *path, uchar_t names[][PATH_LEN])
{
    uchar_t *str = NULL;
    char    *saveptr = NULL;
    uchar_t *token = NULL;
    int      i = 1;

	strcpy((char *)names[0], "/");

    for (str = path ; ; str = NULL, token = NULL, i++)
    {
        token = (uchar_t *)strtok_r((char *)str, "/", &saveptr);
        if (token == NULL)
        {
            break;
        }

        memset(names[i], 0x00, PATH_LEN);
		strcpy((char *)names[i], (const char *)token);
    }

    return i;
}

void key_encode(uchar_t *path, uchar_t *key)
{
    string_t src;
    string_set(src, path);

    string_t dst;
    string_set(dst, key);

    string_base64_encode(&dst, &src);
}

void get_path_keys(uchar_t names[][PATH_LEN], int num, uchar_t keys[][PATH_LEN])
{
    uchar_t sub_path[PATH_LEN] = "";
    uchar_t ancestor_path[PATH_LEN] = "";

    for (int i = 0; i < num; i++)
    {
        if (0 == i) 
		{
		    string_xxsprintf(sub_path, "%s", names[i]);	
		}
        else if (1 == i)
        {
            string_xxsprintf(sub_path, "/%s", names[i]);
        }
        else
        {
            string_xxsprintf(sub_path, "%s/%s", ancestor_path, names[i]);
        }

        string_strncpy(ancestor_path, sub_path, string_strlen(sub_path));

        memset(keys[i], 0x00, PATH_LEN);
		key_encode(sub_path, keys[i]);
    }
}

int get_path_inodes(uchar_t keys[][PATH_LEN], int num, 
	fi_inode_t *finodes[])
{
    for (int i = 0; i < num; i++)
	{
	    fi_store_t *fi = get_store_obj(keys[i]);
		if (NULL == fi) 
		{
		    // parent's inode index
            return i - 1;
		}
		
        finodes[i] = &fi->fin;
    }

	return -1;
}

int is_FsObjectExceed(int num)
{
    int isExceed = DFS_FALSE;
	dfs_lock_errno_t lerr;

	conf_server_t *sconf = (conf_server_t *)dfs_cycle->sconf;

	dfs_atomic_lock_on(&g_fs_object_num_lock, &lerr);
    
    isExceed = g_fs_object_num + num >= sconf->index_num
		? DFS_TRUE : DFS_FALSE;

    dfs_atomic_lock_off(&g_fs_object_num_lock, &lerr);
	
    return isExceed;
}

int inc_FsObjectNum(int num)
{
    dfs_lock_errno_t lerr;

    dfs_atomic_lock_on(&g_fs_object_num_lock, &lerr);
    
    g_fs_object_num += num;

    dfs_atomic_lock_off(&g_fs_object_num_lock, &lerr);

    return DFS_OK;
}

int sub_FsObjectNum(int num)
{
    dfs_lock_errno_t lerr;
    
    dfs_atomic_lock_on(&g_fs_object_num_lock, &lerr);
    
    g_fs_object_num -= num;

    dfs_atomic_lock_off(&g_fs_object_num_lock, &lerr);

    return DFS_OK;
}

int is_InSafeMode()
{
    return DFS_FALSE;
}

int nn_mkdir(task_t *task)
{
    task_queue_node_t *node = queue_data(task, task_queue_node_t, tk);
	
    if (is_InSafeMode()) 
	{
	    task->ret = IN_SAFE_MODE;

		return write_back(node);
	}

	push_task(&paxos_thread->tq, node);
	
    return notice_wake_up(&paxos_thread->tq_notice);
}

int nn_rmr(task_t *task)
{
    task_queue_node_t *node = queue_data(task, task_queue_node_t, tk);
	
    if (is_InSafeMode()) 
	{
	    task->ret = IN_SAFE_MODE;

		return write_back(node);
	}

	push_task(&paxos_thread->tq, node);
	
    return notice_wake_up(&paxos_thread->tq_notice);
}

int nn_ls(task_t *task)
{
	task_queue_node_t *node = queue_data(task, task_queue_node_t, tk);

	fi_store_t *fis = get_store_obj((uchar_t *)task->key);
	if (!fis) 
	{
		task->ret = KEY_NOTEXIST;

		return write_back(node);
	}

	uchar_t path[PATH_LEN] = "";
	get_store_path((uchar_t *)task->key, path);

    pthread_rwlock_rdlock(&g_fcm->cache_rwlock);
	
	fi_inode_t finode = fis->fin;

	pthread_rwlock_unlock(&g_fcm->cache_rwlock);

    if (!is_super(task->user, &dfs_cycle->admin))
    {
		if (check_ancestor_access(path, task, READ_EXECUTE, &finode) != DFS_OK) 
	    {
            task->ret = PERMISSION_DENY;

			return write_back(node);
	    }
    }

	if (!finode.is_directory) 
	{
        task->data_len = sizeof(fi_inode_t);
		task->data = malloc(task->data_len);
		if (NULL == task->data) 
		{
            dfs_log_error(dfs_cycle->error_log, DFS_LOG_ALERT, 0, "malloc err");
		}

		memcpy(task->data, &finode, sizeof(fi_inode_t));
	}
	else 
	{
        pthread_rwlock_rdlock(&g_fcm->cache_rwlock);
	
	    uint64_t children_num = fis->children_num;
	    if (children_num > 0) 
	    {
            task->data_len = children_num * sizeof(fi_inode_t);
            task->data = malloc(task->data_len);
	        if (NULL == task->data) 
	        {
	            dfs_log_error(dfs_cycle->error_log, DFS_LOG_ALERT, 0, 
					"malloc err");
	        }
	    }

        void *pData = task->data;
	
	    queue_t *head = &fis->children;
	    queue_t *entry = queue_next(head);

	    while (children_num > 0) 
	    {
            fi_store_t *fsubdir = queue_data(entry, fi_store_t, me);
		
		    entry = queue_next(entry);
		
		    memcpy(pData, &fsubdir->fin, sizeof(fi_inode_t));
		    pData += sizeof(fi_inode_t);
		
		    children_num--;
	    }

	    pthread_rwlock_unlock(&g_fcm->cache_rwlock);
	}
    
	task->ret = DFS_OK;
	
    return write_back(node);
}

int nn_get_file_info(task_t *task)
{
    return DFS_OK;
}

int nn_create(task_t *task)
{
    task_queue_node_t *node = queue_data(task, task_queue_node_t, tk);
	
    if (is_InSafeMode()) 
	{
	    task->ret = IN_SAFE_MODE;

		return write_back(node);
	}
	
	push_task(&paxos_thread->tq, node);
	
    return notice_wake_up(&paxos_thread->tq_notice);
}

int nn_get_additional_blk(task_t *task)
{
    task_queue_node_t *node = queue_data(task, task_queue_node_t, tk);
	
    if (is_InSafeMode()) 
	{
	    task->ret = IN_SAFE_MODE;

		return write_back(node);
	}
	
	push_task(&paxos_thread->tq, node);
	
    return notice_wake_up(&paxos_thread->tq_notice);
}

int nn_close(task_t *task)
{
    task_queue_node_t *node = queue_data(task, task_queue_node_t, tk);
	
    if (is_InSafeMode()) 
	{
	    task->ret = IN_SAFE_MODE;

		return write_back(node);
	}
	
	push_task(&paxos_thread->tq, node);
	
    return notice_wake_up(&paxos_thread->tq_notice);
}

int nn_rm(task_t *task)
{
    task_queue_node_t *node = queue_data(task, task_queue_node_t, tk);
	
    if (is_InSafeMode()) 
	{
	    task->ret = IN_SAFE_MODE;

		return write_back(node);
	}

	push_task(&paxos_thread->tq, node);
	
    return notice_wake_up(&paxos_thread->tq_notice);
}

int nn_open(task_t *task)
{
    create_blk_info_t  blk_info;
	create_resp_info_t resp_info;

    memset(&resp_info, 0x00, sizeof(create_resp_info_t));
	memset(&blk_info, 0x00, sizeof(create_blk_info_t));
	memcpy(&blk_info, task->data, sizeof(create_blk_info_t));

	task->data_len = 0;
	task->data = NULL;
	
    task_queue_node_t *node = queue_data(task, task_queue_node_t, tk);

	fi_store_t *fi = get_store_obj((uchar_t *)task->key);
	if (!fi) 
	{
        task->ret = KEY_NOTEXIST;
		
		return write_back(node);
	}
	else if (fi->state == KEY_STATE_CREATING) 
	{
        task->ret = KEY_STATE_CREATING;
		
		return write_back(node);
	}

	fi_inode_t fin = fi->fin;

	if (fin.is_directory) 
	{
        task->ret = NOT_FILE;
		
		return write_back(node);
	}

	uchar_t path[PATH_LEN] = "";
	get_store_path((uchar_t *)task->key, path);

	if (!is_super(task->user, &dfs_cycle->admin)) 
	{
        if (check_ancestor_access(path, task, READ_EXECUTE, &fin) != DFS_OK) 
		{
            task->ret = PERMISSION_DENY;

			return write_back(node);
		}
	}
	
	blk_store_t *blk = get_blk_store_obj(fin.blks[0]);

	resp_info.blk_id = blk->id;
	resp_info.blk_sz = blk->size;
	resp_info.namespace_id = dfs_cycle->namespace_id;

	resp_info.dn_num = 1;
	strcpy(resp_info.dn_ips[0], blk->dn_ip);
	strcpy(resp_info.dn_ips[1], "");
	strcpy(resp_info.dn_ips[2], "");

	task->data_len = sizeof(create_resp_info_t);
	task->data = malloc(task->data_len);
	if (NULL == task->data) 
	{
        dfs_log_error(dfs_cycle->error_log, DFS_LOG_ALERT, 0, "malloc err");
	}
	
	memcpy(task->data, &resp_info, task->data_len);

	task->ret = SUCC;
	
	return write_back(node);
}

int update_fi_cache_mgmt(const uint64_t llInstanceID, 
	const std::string & sPaxosValue, void *data)
{
    fi_inode_t fin;
	memset(&fin, 0x00, sizeof(fi_inode_t));
		
    string str;
    LogOperator lopr;
	lopr.ParseFromString(sPaxosValue);

	int optype = lopr.optype();
	
	switch (optype)
    {
    case NN_MKDIR:
		fin.uid = llInstanceID;
		strcpy(fin.key, lopr.mutable_mkr()->key().c_str());
		fin.permission = lopr.mutable_mkr()->permission();
		strcpy(fin.owner, lopr.mutable_mkr()->owner().c_str());
		strcpy(fin.group, lopr.mutable_mkr()->group().c_str());
		fin.modification_time = lopr.mutable_mkr()->modification_time();
		fin.is_directory = DFS_TRUE;
		
		update_fi_mkdir(&fin);
		break;

	case NN_RMR:
		strcpy(fin.key, lopr.mutable_rmr()->key().c_str());
		fin.modification_time = lopr.mutable_rmr()->modification_time();

		update_fi_rmr(&fin);
		break;
		
	case NN_GET_FILE_INFO:
		break;

	case NN_CREATE:
		fin.uid = llInstanceID;
		strcpy(fin.key, lopr.mutable_cre()->key().c_str());
		fin.permission = lopr.mutable_cre()->permission();
		strcpy(fin.owner, lopr.mutable_cre()->owner().c_str());
		strcpy(fin.group, lopr.mutable_cre()->group().c_str());
		fin.modification_time = lopr.mutable_cre()->modification_time();
		fin.blk_size = lopr.mutable_cre()->blk_sz();
		fin.blk_replication = lopr.mutable_cre()->blk_rep();
		fin.is_directory = DFS_FALSE;
		
		update_fi_create(&fin, lopr.mutable_cre()->blk_id(), data);
		break;

	case NN_GET_ADDITIONAL_BLK:
		fin.uid = llInstanceID;
		strcpy(fin.key, lopr.mutable_gab()->key().c_str());
		fin.blk_size = lopr.mutable_gab()->blk_sz();
		fin.blk_replication = lopr.mutable_gab()->blk_rep();

		update_fi_get_additional_blk(&fin, lopr.mutable_gab()->blk_id());
		break;

	case NN_CLOSE:
		strcpy(fin.key, lopr.mutable_cle()->key().c_str());
		fin.modification_time = lopr.mutable_cle()->modification_time();
		fin.length = lopr.mutable_cle()->len();
		fin.blk_replication = lopr.mutable_cle()->blk_rep();

		update_fi_close(&fin);
		break;

	case NN_RM:
		strcpy(fin.key, lopr.mutable_rm()->key().c_str());
		fin.modification_time = lopr.mutable_rm()->modification_time();

		update_fi_rm(&fin);
		break;

	case NN_OPEN:
		break;
		
	default:
		dfs_log_error(dfs_cycle->error_log, DFS_LOG_ALERT, 0, 
			"unknown optype: ", optype);
		return DFS_ERROR;
	}
	
    return DFS_OK;
}

static void get_parent_key(uchar_t path[], uchar_t key[])
{
    uchar_t *pTemp = path;
	uchar_t *pPath = (uchar_t *)strrchr((const char *)path, '/');
    int pIndex = pPath - pTemp;

    uchar_t pName[PATH_LEN] = "";
    if (0 == pIndex) 
	{
	    // /home
	    memcpy(pName, "/", 1);
	}
	else
	{
        memcpy(pName, pTemp, pIndex);
	}

    key_encode(pName, key);
}

static int update_fi_mkdir(fi_inode_t *fin)
{
    pthread_rwlock_wrlock(&g_fcm->cache_rwlock);
	
    fi_store_t *fis = (fi_store_t *)mem_get0(g_fcm->mem_mgmt.free_mblks);

    queue_init(&fis->ckp);
	queue_init(&fis->me);
	queue_init(&fis->children);

	memcpy(&fis->fin, fin, sizeof(fi_inode_t));

	fis->ln.key = fis->fin.key;
    fis->ln.len = string_strlen(fis->fin.key);
    fis->ln.next = NULL;

	dfs_hashtable_join(g_fcm->fi_htable, &fis->ln);

	uchar_t path[PATH_LEN] = "";
	get_store_path((uchar_t *)fin->key, path);

	if (0 != string_strncmp("/", path, string_strlen(path)))
	{
		uchar_t pKey[PATH_LEN] = "";
		get_parent_key(path, pKey);

		fi_store_t *fparent = (fi_store_t *)dfs_hashtable_lookup(g_fcm->fi_htable, 
			(void *)pKey, string_strlen(pKey));

		fparent->fin.modification_time = fin->modification_time;
	
	    queue_insert_tail(&fparent->children, &fis->me);
		fparent->children_num++;
	}

	queue_insert_tail(&g_checkpoint_q, &fis->ckp);

	pthread_rwlock_unlock(&g_fcm->cache_rwlock);

	inc_FsObjectNum(1);
	
    return DFS_OK;
}

static int update_fi_rmr(fi_inode_t *fin)
{
    uchar_t path[PATH_LEN] = "";
	get_store_path((uchar_t *)fin->key, path);

	uchar_t pKey[PATH_LEN] = "";
	get_parent_key(path, pKey);

	pthread_rwlock_wrlock(&g_fcm->cache_rwlock);

	fi_store_t *fparent = (fi_store_t *)dfs_hashtable_lookup(g_fcm->fi_htable, 
		(void *)pKey, string_strlen(pKey));

	fparent->fin.modification_time = fin->modification_time;

	fi_store_t *fcurrent = (fi_store_t *)dfs_hashtable_lookup(g_fcm->fi_htable, 
		(void *)fin->key, string_strlen(fin->key));
	
	int num = clear_children(&fcurrent->children, fcurrent->children_num);

	dfs_hashtable_remove_link(g_fcm->fi_htable, &fcurrent->ln);

	queue_remove(&fcurrent->me);
	queue_remove(&fcurrent->ckp);

	fi_store_destroy(fcurrent);

	num++;

	fparent->children_num--;

	pthread_rwlock_unlock(&g_fcm->cache_rwlock);

	sub_FsObjectNum(num);

    return DFS_OK;
}

static int clear_children(queue_t *head, uint64_t num)
{
    int file_num = 0;
	int dir_num = 0;
	
    queue_t *entry = queue_next(head);
	
    while (num > 0) 
	{
        fi_store_t *fis = queue_data(entry, fi_store_t, me);

		entry = queue_next(entry);

		queue_remove(&fis->me);
		queue_remove(&fis->ckp);

		if (fis->fin.is_directory) 
		{
		    dir_num += clear_children(&fis->children, fis->children_num); 
            dir_num++;
		}
		else 
		{
		    uint64_t del_blks[BLK_LIMIT];
			memcpy(&del_blks, &fis->fin.blks, sizeof(fis->fin.blks));

			for (int i = 0; i < BLK_LIMIT; i++) 
	        {
                if (del_blks[i] > 0) 
		        {
                    block_object_del(del_blks[i]);
		        }
	        }
	
		    file_num++;
		}

        dfs_hashtable_remove_link(g_fcm->fi_htable, &fis->ln);
        fi_store_destroy(fis);

		num--;
	}
	
    return file_num + dir_num;
}

int do_checkpoint()
{
	dfs_log_error(dfs_cycle->error_log, DFS_LOG_INFO, 0, 
		"do_checkpoint start, lastCheckpointInstanceID: %ld", 
		lastCheckpointInstanceID);

    if (mv_current() != DFS_OK) 
	{
        return DFS_ERROR;
	}
    
	if (save_image() != DFS_OK) 
	{
        return DFS_ERROR;
	}

	if (save_checkpoinID() != DFS_OK) 
	{
        return DFS_ERROR;
	}

	if (mv_last_checkpoint() != DFS_OK) 
	{
        return DFS_ERROR;
	}

	set_checkpoint_instanceID(lastCheckpointInstanceID);
	
    return DFS_OK;
}

static int mv_current()
{
    conf_server_t *conf = (conf_server_t *)dfs_cycle->sconf;
	
    char src[PATH_LEN] = {0};
	char dst[PATH_LEN] = {0};
	string_xxsprintf((uchar_t *)src, "%s/current", conf->fsimage_dir.data);
	string_xxsprintf((uchar_t *)dst, "%s/lastcheckpoint.tmp", 
		conf->fsimage_dir.data);
	
    // mv current to lastcheckpoint.tmp
	if (mkdir(dst, S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH) != DFS_OK) 
	{
	    dfs_log_error(dfs_cycle->error_log, DFS_LOG_ERROR, errno, 
			"mkdir %s err", dst);
		
	    return DFS_ERROR;
	}

	DIR *dp = NULL;
    struct dirent *entry = NULL;
	
    if ((dp = opendir(src)) == NULL) 
	{
		dfs_log_error(dfs_cycle->error_log, DFS_LOG_ERROR, errno, 
			"opendir %s err", src);
		
        return DFS_ERROR;
    }

	while ((entry = readdir(dp)) != NULL) 
	{
		if (entry->d_type == 8)
		{
		    // file
            char sBuf[PATH_LEN] = {0};
			char dBuf[PATH_LEN] = {0};
 		    sprintf(sBuf, "%s/%s", src, entry->d_name);
			sprintf(dBuf, "%s/%s", dst, entry->d_name);
 
 		    copy_file(sBuf, dBuf);
		}
		else if (0 != strcmp(entry->d_name, ".") 
			&& 0 != strcmp(entry->d_name, ".."))
		{
            // sub-dir
		}
    }
	
	closedir(dp);
	
    return DFS_OK;
}

static int copy_file(const char *src, const char *dst)
{
    int         rs = DFS_ERROR;
    int         in_fd = -1;
    int         out_fd = -1;
    int         ws = 0;
    size_t      fsize = 0;
    struct stat sb;

    in_fd = open(src, O_RDONLY);
    if (in_fd < 0) 
	{
        dfs_log_error(dfs_cycle->error_log, DFS_LOG_ERROR, errno, 
			"open file %s err", src);
        
        goto out;
    }
    
    fstat(in_fd, &sb);
    fsize = sb.st_size;

    out_fd = open(dst, O_RDWR | O_CREAT | O_TRUNC, 0664);
    if (out_fd < 0) 
	{    
        dfs_log_error(dfs_cycle->error_log, DFS_LOG_ERROR, errno, 
			"open file %s err", dst);
        
        goto out;
    }

    while (fsize > 0) 
	{
        ws = sendfile(out_fd, in_fd, NULL, fsize);
        if (ws < 0) 
		{
            if (errno == DFS_EAGAIN || errno == DFS_EINTR) 
			{  
                continue;
            }

            dfs_log_error(dfs_cycle->error_log, DFS_LOG_ERROR, 
				errno, "sendfile err");
            
            goto out;
        }

        fsize -= ws;
    }

    rs = DFS_OK;
	
    dfs_log_error(dfs_cycle->error_log, DFS_LOG_INFO, 0, 
		"copy %s to %s successfully.", src, dst);

out:
    if (in_fd > 0) 
	{
        close(in_fd);
        in_fd = -1;
    }

    if (out_fd > 0) 
	{
        close(out_fd);
        out_fd = -1;
    }

    return rs;
}

static int mv_last_checkpoint()
{
    conf_server_t *conf = (conf_server_t *)dfs_cycle->sconf;
	
    char src[PATH_LEN] = {0};
	char dst[PATH_LEN] = {0};
	string_xxsprintf((uchar_t *)src, "%s/lastcheckpoint.tmp", 
		conf->fsimage_dir.data);
	string_xxsprintf((uchar_t *)dst, "%s/previous.checkpoint", 
		conf->fsimage_dir.data);

	if (access(dst, F_OK) == DFS_OK) 
	{
	    delete_dir(dst);
	}
	
    // mv lastcheckpoint.tmp to previous.checkpoint
    if (rename(src, dst) != DFS_OK) 
	{
	    dfs_log_error(dfs_cycle->error_log, DFS_LOG_ERROR, errno, 
			"rename %s to %s err", src, dst);
		
	    return DFS_ERROR;
	}
    
    return DFS_OK;
}

static int delete_dir(const char *dir)
{
    DIR *dp = NULL;
    struct dirent *entry = NULL;
	
    if ((dp = opendir(dir)) == NULL) 
	{
		dfs_log_error(dfs_cycle->error_log, DFS_LOG_ERROR, errno, 
			"opendir %s err", dir);
		
        return DFS_ERROR;
    }

	while ((entry = readdir(dp)) != NULL) 
	{
		if (entry->d_type == 8)
		{
		    // file
            char sBuf[PATH_LEN] = {0};
 		    sprintf(sBuf, "%s/%s", dir, entry->d_name);
 
 		    unlink(sBuf);
		}
		else if (0 != strcmp(entry->d_name, ".") 
			&& 0 != strcmp(entry->d_name, ".."))
		{
		    // sub-dir
		    char sBuf[PATH_LEN] = {0};
			sprintf(sBuf, "%s/%s", dir, entry->d_name);
			
            delete_dir(sBuf);
		}
    }
	
	closedir(dp);

	rmdir(dir);
	
    return DFS_OK;
}

int load_image()
{
    dfs_log_error(dfs_cycle->error_log, DFS_LOG_INFO, 0, 
		"load_image start, lastCheckpointInstanceID: %l", 
		lastCheckpointInstanceID);

	conf_server_t *conf = (conf_server_t *)dfs_cycle->sconf;

	char image_name[PATH_LEN] = {0};
	string_xxsprintf((uchar_t *)image_name, "%s/current/fsimage", 
		conf->fsimage_dir.data);
	
	int fd = open(image_name, O_RDONLY);
	if (fd < 0) 
	{
	    dfs_log_error(dfs_cycle->error_log, DFS_LOG_WARN, errno, 
			"open[%s] err", image_name);
		
        return DFS_ERROR;
	}

    fi_inode_t fin;
	bzero(&fin, sizeof(fi_inode_t));
	
	while (read(fd, &fin, sizeof(fi_inode_t)) > 0) 
	{
	    update_fi_mkdir(&fin);
	}

    read_checkpoinID();
    set_checkpoint_instanceID(lastCheckpointInstanceID);
	
    return DFS_OK;
}

static int save_image()
{
    conf_server_t *conf = (conf_server_t *)dfs_cycle->sconf;
    rb_msec_t start_time = dfs_current_msec;
	uint64_t uid = 0;
	
    char image_name[PATH_LEN] = {0};
	string_xxsprintf((uchar_t *)image_name, "%s/current/fsimage", 
		conf->fsimage_dir.data);
	
	int fd = open(image_name, O_RDWR | O_CREAT | O_TRUNC, 0664);
	if (fd < 0) 
	{
	    dfs_log_error(dfs_cycle->error_log, DFS_LOG_ERROR, errno, 
			"open[%s] err", image_name);
		
        return DFS_ERROR;
	}

	queue_t qhead;
	queue_init(&qhead);

	pthread_rwlock_rdlock(&g_fcm->cache_rwlock);
	
	if (!queue_empty(&g_checkpoint_q)) 
	{
        qhead.next = g_checkpoint_q.next;
        qhead.prev = g_checkpoint_q.prev;
        
        qhead.next->prev = &qhead;
        qhead.prev->next = &qhead;
	}
	
	pthread_rwlock_unlock(&g_fcm->cache_rwlock);
	
	queue_t *head = &qhead;
	queue_t *entry = queue_next(head);
	
	while (head != entry) 
	{
	    fi_store_t *fis = queue_data(entry, fi_store_t, ckp);
		fi_inode_t fii = fis->fin;

		if (fii.modification_time > start_time) 
		{
			break;
		}

	    if (write(fd, &fii, sizeof(fi_inode_t)) < 0) 
		{
		    dfs_log_error(dfs_cycle->error_log, DFS_LOG_ERROR, errno, 
				"write[%s] err", image_name);
			
            return DFS_ERROR;
		}

		uid = fii.uid;

		entry = queue_next(entry);
	}

	close(fd);

	lastCheckpointInstanceID = uid;
	
    return DFS_OK;
}

static int save_checkpoinID()
{
    conf_server_t *conf = (conf_server_t *)dfs_cycle->sconf;
	
    char ckp_name[PATH_LEN] = {0};
	string_xxsprintf((uchar_t *)ckp_name, "%s/current/ckpid", 
		conf->fsimage_dir.data);
	
	int fd = open(ckp_name, O_RDWR | O_CREAT | O_TRUNC, 0664);
	if (fd < 0) 
	{
	    dfs_log_error(dfs_cycle->error_log, DFS_LOG_ERROR, errno, 
			"open[%s] err", ckp_name);
		
        return DFS_ERROR;
	}

	if (write(fd, &lastCheckpointInstanceID, sizeof(uint64_t)) < 0) 
    {
		dfs_log_error(dfs_cycle->error_log, DFS_LOG_ERROR, errno, 
			"write[%s] err", ckp_name);

		return DFS_ERROR;
	}

	close(fd);
	
    return DFS_OK;
}

static int read_checkpoinID()
{
    conf_server_t *conf = (conf_server_t *)dfs_cycle->sconf;
	
    char ckp_name[PATH_LEN] = {0};
	string_xxsprintf((uchar_t *)ckp_name, "%s/current/ckpid", 
		conf->fsimage_dir.data);
	
	int fd = open(ckp_name, O_RDONLY);
	if (fd < 0) 
	{
	    dfs_log_error(dfs_cycle->error_log, DFS_LOG_WARN, errno, 
			"open %s err", ckp_name);
		
        return DFS_ERROR;
	}

	if (read(fd, &lastCheckpointInstanceID, sizeof(uint64_t)) < 0) 
    {
		dfs_log_error(dfs_cycle->error_log, DFS_LOG_ERROR, errno, 
			"read %s err", ckp_name);

		close(fd);

		return DFS_ERROR;
	}

	close(fd);
	
    return DFS_OK;
}

static int update_fi_create(fi_inode_t *fin, uint64_t blk_id, void *data)
{	
    dfs_thread_t *thread = (dfs_thread_t *)data;
	
    pthread_rwlock_wrlock(&g_fcm->cache_rwlock);
	
    fi_store_t *fis = (fi_store_t *)mem_get0(g_fcm->mem_mgmt.free_mblks);

    queue_init(&fis->ckp);
	queue_init(&fis->me);
	fis->state = KEY_STATE_CREATING;

	memcpy(&fis->fin, fin, sizeof(fi_inode_t));

	for (int i = 0; i < BLK_LIMIT; i++) 
	{
		fis->fin.blks[i] = -1;
	}

	fis->fin.blks[0] = blk_id;

	fis->ln.key = fis->fin.key;
    fis->ln.len = string_strlen(fis->fin.key);
    fis->ln.next = NULL;

	dfs_hashtable_join(g_fcm->fi_htable, &fis->ln);

    if (thread != NULL) 
	{
	    fis->thread = thread;
        fis->timer_ev.data = fis;
	    fis->timer_ev.handler = fi_create_timeout;
	    event_timer_add(&fis->thread->event_timer, 
			&fis->timer_ev, FI_CREATE_TIME_OUT);
	}

	pthread_rwlock_unlock(&g_fcm->cache_rwlock);

	inc_FsObjectNum(1);
	
    return DFS_OK;
}

static void fi_create_timeout(event_t *ev)
{
    fi_store_t *fis = NULL;

	fis = (fi_store_t *)ev->data;

	dfs_hashtable_remove_link(g_fcm->fi_htable, &fis->ln);

	//queue_remove(&fis->me);
	//queue_remove(&fis->ckp);

	fi_store_destroy(fis);
}

static int update_fi_get_additional_blk(fi_inode_t *fin, 
	uint64_t blk_id)
{
    fi_store_t *fis = get_store_obj((uchar_t *)fin->key);

    pthread_rwlock_wrlock(&g_fcm->cache_rwlock);
	
	for (int i = 0; i < BLK_LIMIT; i++) 
	{
		if (-1 == fis->fin.blks[i]) 
		{
            fis->fin.blks[i] = blk_id;

			break;
		}
	}

    if (fis->thread != NULL) 
	{
	    event_timer_add(&fis->thread->event_timer, 
			&fis->timer_ev, FI_CREATE_TIME_OUT);
    }

	pthread_rwlock_unlock(&g_fcm->cache_rwlock);
	
    return DFS_OK;
}

static int update_fi_close(fi_inode_t *fin)
{
    fi_store_t *fis = get_store_obj((uchar_t *)fin->key);

    pthread_rwlock_wrlock(&g_fcm->cache_rwlock);

    if (fis->thread != NULL) 
	{
	    event_timer_del(&fis->thread->event_timer, &fis->timer_ev);
	    fis->thread = NULL;
    }

	fis->state = KEY_STATE_OK;
	fis->fin.modification_time = fin->modification_time;
	fis->fin.length = fin->length;
	fis->fin.blk_replication = fin->blk_replication;

	uchar_t path[PATH_LEN] = "";
	get_store_path((uchar_t *)fin->key, path);

	uchar_t pKey[PATH_LEN] = "";
	get_parent_key(path, pKey);

	fi_store_t *fparent = (fi_store_t *)dfs_hashtable_lookup(g_fcm->fi_htable, 
		(void *)pKey, string_strlen(pKey));

	fparent->fin.modification_time = fin->modification_time;
	
	queue_insert_tail(&fparent->children, &fis->me);
	fparent->children_num++;

	queue_insert_tail(&g_checkpoint_q, &fis->ckp);

	pthread_rwlock_unlock(&g_fcm->cache_rwlock);
	
    return DFS_OK;
}

static int update_fi_rm(fi_inode_t *fin)
{
    uchar_t path[PATH_LEN] = "";
	get_store_path((uchar_t *)fin->key, path);

	uchar_t pKey[PATH_LEN] = "";
	get_parent_key(path, pKey);

	pthread_rwlock_wrlock(&g_fcm->cache_rwlock);

	fi_store_t *fparent = (fi_store_t *)dfs_hashtable_lookup(g_fcm->fi_htable, 
		(void *)pKey, string_strlen(pKey));

	fparent->fin.modification_time = fin->modification_time;

	fi_store_t *fcurrent = (fi_store_t *)dfs_hashtable_lookup(g_fcm->fi_htable, 
		(void *)fin->key, string_strlen(fin->key));

	uint64_t del_blks[BLK_LIMIT];
	memcpy(&del_blks, &fcurrent->fin.blks, sizeof(fcurrent->fin.blks));

	dfs_hashtable_remove_link(g_fcm->fi_htable, &fcurrent->ln);

	queue_remove(&fcurrent->me);
	queue_remove(&fcurrent->ckp);

	fi_store_destroy(fcurrent);

	fparent->children_num--;

	pthread_rwlock_unlock(&g_fcm->cache_rwlock);

	sub_FsObjectNum(1);

	for (int i = 0; i < BLK_LIMIT; i++) 
	{
        if (del_blks[i] > 0) 
		{
            block_object_del(del_blks[i]);
		}
	}

    return DFS_OK;
}


