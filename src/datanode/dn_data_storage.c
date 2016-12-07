#include "dn_data_storage.h"
#include "dfs_types.h"
#include "dfs_math.h"
#include "dfs_memory.h"
#include "dfs_commpool.h"
#include "dfs_mblks.h"
#include "dn_conf.h"
#include "dn_time.h"
#include "dn_process.h"
#include "dn_ns_service.h"

#define BLK_NUM_IN_DN 100000

uint32_t blk_scanner_running = DFS_TRUE;

static queue_t g_storage_dir_q;
static int     g_storage_dir_n = 0;
static char    g_last_version[56] = "";

static blk_cache_mgmt_t *g_dn_bcm = NULL;

static int init_storage_dirs(cycle_t *cycle);
static int create_storage_dirs(cycle_t *cycle);
static int check_version(char *path);
static int check_namespace(char *path, int64_t namespaceID);
static int create_storage_subdirs(char *path);
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
static int get_disk_id(long block_id, char *path);
static int recv_blk_report(dn_request_t *r);
static int scan_current_dir(char *dir);
static void get_namespace_id(char *src, char *id);
static int scan_namespace_dir(char *dir, long namespace_id);
static int scan_subdir(char *dir, long namespace_id);
static int scan_subdir_subdir(char *dir, long namespace_id);
static void get_blk_id(char *src, char *id);

int dn_data_storage_master_init(cycle_t *cycle)
{
	cycle->cfs = pool_alloc(cycle->pool, sizeof(cfs_t));
	if (!cycle->cfs) 
	{
        dfs_log_error(dfs_cycle->error_log, DFS_LOG_ERROR, errno, 
			"pool_alloc err");

		return DFS_ERROR;
	}

	if (cfs_setup(cycle->pool, (cfs_t *)cycle->cfs, cycle->error_log) 
		!= DFS_OK) 
	{
        return DFS_ERROR;
    }
	
    return DFS_OK;
}

int dn_data_storage_worker_init(cycle_t *cycle)
{
    queue_init(&g_storage_dir_q);

	if (init_storage_dirs(cycle) != DFS_OK) 
	{
	    return DFS_ERROR;
	}

	if (create_storage_dirs(cycle) != DFS_OK) 
	{
	    return DFS_ERROR;
	}

	if (cfs_prepare_work(cycle) != DFS_OK) 
	{
        return DFS_ERROR;
    }

	g_dn_bcm = blk_cache_mgmt_new_init();
    if (!g_dn_bcm) 
	{
        return DFS_ERROR;
    }

	blk_report_queue_init();
	
    return DFS_OK;
}

int dn_data_storage_worker_release(cycle_t *cycle)
{
    blk_cache_mgmt_release(g_dn_bcm);
	g_dn_bcm = NULL;

	blk_report_queue_release();
	
    return DFS_OK;
}

int dn_data_storage_thread_init(dfs_thread_t *thread)
{
    if (cfs_fio_manager_init(dfs_cycle, &thread->fio_mgr) != DFS_OK) 
	{
        return DFS_ERROR;
	}

	if (cfs_notifier_init(&thread->faio_notify) != DFS_OK) 
	{
        return DFS_ERROR;
    }

    return cfs_ioevent_init(&thread->io_events);
}

static int init_storage_dirs(cycle_t *cycle)
{
    conf_server_t *sconf = (conf_server_t *)cycle->sconf;
    uchar_t       *str = sconf->data_dir.data;
    char          *saveptr = NULL;
    uchar_t       *token = NULL;
	char           dir[PATH_LEN] = "";

    for (int i = 0; ; str = NULL, token = NULL, i++)
    {
        token = (uchar_t *)strtok_r((char *)str, ",", &saveptr);
        if (token == NULL)
        {
            break;
        }

		storage_dir_t *sd = (storage_dir_t *)pool_alloc(cycle->pool, 
			sizeof(storage_dir_t));
		if (!sd) 
		{
            dfs_log_error(dfs_cycle->error_log, DFS_LOG_ERROR, errno, 
				"pool_alloc err");

			return DFS_ERROR;
		}

        sd->id = i;
		string_xxsprintf((uchar_t *)dir, "%s/current", token);
		strcpy(sd->current, dir);
		queue_insert_tail(&g_storage_dir_q, &sd->me);
		g_storage_dir_n++;
    }

    return DFS_OK;
}

static int create_storage_dirs(cycle_t *cycle)
{
	queue_t *head = &g_storage_dir_q;
	queue_t *entry = queue_next(head);

    while (head != entry)
    {
        storage_dir_t *sd = queue_data(entry, storage_dir_t, me);
		
		entry = queue_next(entry);

	    if (access(sd->current, F_OK) != DFS_OK) 
	    {
	        if (mkdir(sd->current, 
				S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH) != DFS_OK) 
	        {
	            dfs_log_error(dfs_cycle->error_log, DFS_LOG_ERROR, errno, 
					"mkdir %s err", sd->current);
		
	            return DFS_ERROR;
	        }
	    }
    }
	
    return DFS_OK;
}

int setup_ns_storage(dfs_thread_t *thread)
{
    char path[PATH_LEN] = "";
	
    queue_t *head = &g_storage_dir_q;
	queue_t *entry = queue_next(head);

    while (head != entry)
    {
        storage_dir_t *sd = queue_data(entry, storage_dir_t, me);
		
		entry = queue_next(entry);

        sprintf(path, "%s/VERSION", sd->current);
        if (check_version(path) != DFS_OK) 
		{
            exit(PROCESS_KILL_EXIT);
		}

        sprintf(path, "%s/NS-%ld", sd->current, thread->ns_info.namespaceID);
		if (check_namespace(path, thread->ns_info.namespaceID) != DFS_OK) 
		{
            exit(PROCESS_KILL_EXIT);
		}
	}
	
    return DFS_OK;
}

static int check_version(char *path)
{
    if (access(path, F_OK) != DFS_OK) 
	{
	    int fd = open(path, O_RDWR | O_CREAT | O_TRUNC, 0664);
	    if (fd < 0) 
	    {
		    dfs_log_error(dfs_cycle->error_log, DFS_LOG_ERROR, errno, 
				"open %s err", path);
		
            return DFS_ERROR;
	    }

        if (0 == strcmp(g_last_version, "")) 
		{
		    sprintf(g_last_version, "DS-%s-%ld", 
				dfs_cycle->listening_ip, dfs_current_msec);
		}
		
		char version[128] = "";
	    sprintf(version, "storageID=%s\n", g_last_version);

		if (write(fd, version, strlen(version)) < 0) 
        {
		    dfs_log_error(dfs_cycle->error_log, DFS_LOG_ERROR, errno, 
				"write %s err", path);

			close(fd);

		    return DFS_ERROR;
	    }

	    close(fd);
	}
	else 
	{
		int fd = open(path, O_RDONLY);
	    if (fd < 0) 
	    {
		    dfs_log_error(dfs_cycle->error_log, DFS_LOG_ALERT, errno, 
				"open %s err", path);
		
            return DFS_ERROR;
	    }

        char rBuf[128] = "";
	    if (read(fd, rBuf, sizeof(rBuf)) < 0) 
        {
		    dfs_log_error(dfs_cycle->error_log, DFS_LOG_ERROR, errno, 
				"read %s err", path);

		    close(fd);

		    return DFS_ERROR;
	    }

        char version[128] = "";
	    sscanf(rBuf, "%*[^=]=%s", version);

	    close(fd);

		if (0 == strcmp(g_last_version, "")) 
		{
            strcpy(g_last_version, version);
		}
		else if (0 != strcmp(g_last_version, version)) 
		{
		    dfs_log_error(dfs_cycle->error_log, DFS_LOG_ERROR, 0, 
				"storageID in %s is incompatible with others.", path);
			
		    return DFS_ERROR;
		}
	}
	
    return DFS_OK;
}

static int check_namespace(char *path, int64_t namespaceID)
{
    char bbwDir[PATH_LEN] = "";
    char curDir[PATH_LEN] = "";
	char verDir[PATH_LEN] = "";
	
    if (access(path, F_OK) != DFS_OK) 
	{
	    if (mkdir(path, S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH) != DFS_OK) 
	    {
	        dfs_log_error(dfs_cycle->error_log, DFS_LOG_ERROR, errno, 
				"mkdir %s err", path);
		
	        return DFS_ERROR;
	    }

		sprintf(bbwDir, "%s/blocksBeingWritten", path);
		if (mkdir(bbwDir, S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH) != DFS_OK) 
	    {
	        dfs_log_error(dfs_cycle->error_log, DFS_LOG_ERROR, errno, 
				"mkdir %s err", bbwDir);
		
	        return DFS_ERROR;
	    }

        sprintf(curDir, "%s/current", path);
		if (mkdir(curDir, S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH) != DFS_OK) 
	    {
	        dfs_log_error(dfs_cycle->error_log, DFS_LOG_ERROR, errno, 
				"mkdir %s err", curDir);
		
	        return DFS_ERROR;
	    }

		sprintf(verDir, "%s/VERSION", curDir);
        int fd = open(verDir, O_RDWR | O_CREAT | O_TRUNC, 0664);
	    if (fd < 0) 
	    {
		    dfs_log_error(dfs_cycle->error_log, DFS_LOG_ERROR, errno, 
				"open %s err", verDir);
		
            return DFS_ERROR;
	    }

		char version[128] = "";
	    sprintf(version, "namespaceID=%ld\n", namespaceID);

		if (write(fd, version, strlen(version)) < 0) 
        {
		    dfs_log_error(dfs_cycle->error_log, DFS_LOG_ERROR, errno, 
				"write %s err", verDir);

			close(fd);

		    return DFS_ERROR;
	    }

	    close(fd);

		create_storage_subdirs(curDir);
	}
	else 
	{
	    sprintf(verDir, "%s/current/VERSION", path);
	
        int fd = open(verDir, O_RDONLY);
	    if (fd < 0) 
	    {
		    dfs_log_error(dfs_cycle->error_log, DFS_LOG_ALERT, errno, 
				"open %s err", verDir);
		
            return DFS_ERROR;
	    }

        char rBuf[128] = "";
	    if (read(fd, rBuf, sizeof(rBuf)) < 0) 
        {
		    dfs_log_error(dfs_cycle->error_log, DFS_LOG_ERROR, errno, 
				"read %s err", verDir);

		    close(fd);

		    return DFS_ERROR;
	    }

        int64_t dn_ns_id = 0;
	    sscanf(rBuf, "%*[^=]=%ld", &dn_ns_id);

	    close(fd);

		if (dn_ns_id != namespaceID) 
		{
		    dfs_log_error(dfs_cycle->error_log, DFS_LOG_ERROR, 0, 
				"Incompatible namespaceIDs in %s, "
				"namenode namespaceID = %ld, datanode namespaceID = %ld", 
				verDir, namespaceID, dn_ns_id);
			
		    return DFS_ERROR;
		}
	}
	
    return DFS_OK;
}

static int create_storage_subdirs(char *path)
{
    char subDir[PATH_LEN] = "";
	char ssubDir[PATH_LEN] = "";

	for (int i = 0; i < SUBDIR_LEN; i++) 
	{	
        sprintf(subDir, "%s/subdir%d", path, i);
		if (access(subDir, F_OK) != DFS_OK) 
		{
            if (mkdir(subDir, S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH) != DFS_OK) 
	        {
	            dfs_log_error(dfs_cycle->error_log, DFS_LOG_ERROR, errno, 
					"mkdir %s err", subDir);
		
	            return DFS_ERROR;
	        }

			for (int j = 0; j < SUBDIR_LEN; j++) 
			{
                sprintf(ssubDir, "%s/subdir%d", subDir, j);
				if (mkdir(ssubDir, S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH) 
					!= DFS_OK) 
	            {
	                dfs_log_error(dfs_cycle->error_log, DFS_LOG_ERROR, errno, 
						"mkdir %s err", ssubDir);
		
	                return DFS_ERROR;
	            }
			}
		}
	}
	
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

    return mem_mblks_new(block_info_t, count, &mblk_param);
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

block_info_t *block_object_get(long id)
{
    pthread_rwlock_rdlock(&g_dn_bcm->cache_rwlock);

	block_info_t *blk = (block_info_t *)dfs_hashtable_lookup(g_dn_bcm->blk_htable, 
		&id, sizeof(id));

	pthread_rwlock_unlock(&g_dn_bcm->cache_rwlock);
	
    return blk;
}

int block_object_add(char *path, long ns_id, long blk_id)
{
    block_info_t *blk = NULL;

	blk = block_object_get(blk_id);
	if (blk) 
	{
	    // check diff
        return DFS_OK;
	}

	struct stat sb;
	stat(path, &sb);
	long blk_sz = sb.st_size;

	pthread_rwlock_wrlock(&g_dn_bcm->cache_rwlock);

	blk = (block_info_t *)mem_get0(g_dn_bcm->mem_mgmt.free_mblks);
	if (!blk)
	{
	    dfs_log_error(dfs_cycle->error_log, DFS_LOG_ALERT, 0, "mem_get0 err");
	}

	queue_init(&blk->me);
	
    blk->id = blk_id;
	blk->size = blk_sz;
	strcpy(blk->path, path);

	blk->ln.key = &blk->id;
    blk->ln.len = sizeof(blk->id);
    blk->ln.next = NULL;

	dfs_hashtable_join(g_dn_bcm->blk_htable, &blk->ln);

	pthread_rwlock_unlock(&g_dn_bcm->cache_rwlock);
	
    notify_blk_report(blk);
	
    return DFS_OK;
}

int block_object_del(long blk_id)
{
    block_info_t *blk = NULL;

	blk = block_object_get(blk_id);
	if (!blk) 
	{
        return DFS_ERROR;
	}

	unlink(blk->path);
	
	pthread_rwlock_wrlock(&g_dn_bcm->cache_rwlock);

    dfs_hashtable_remove_link(g_dn_bcm->blk_htable, &blk->ln);

	mem_put(blk);

	pthread_rwlock_unlock(&g_dn_bcm->cache_rwlock);
    
    return DFS_OK;
}

int block_read(dn_request_t *r, file_io_t *fio)
{
    return DFS_OK;
}

void io_lock(volatile uint64_t *lock)
{
    uint64_t l;

    do 
	{
        l = *lock;
    } while (!CAS(lock, l, *lock + 1));
}

uint64_t io_unlock(volatile uint64_t *lock)
{
    uint64_t l;

    do 
	{
        l = *lock;
    } while (!CAS(lock, l, *lock - 1));

    return l - 1;
}

int io_lock_zero(volatile uint64_t *lock)
{
    return CAS(lock, 0, 0);
}

int get_block_temp_path(dn_request_t *r)
{
    char tmpDir[PATH_LEN] = "";
	char curDir[PATH_LEN] = "";

	get_disk_id(r->header.block_id, curDir);
	sprintf(tmpDir, "%s/NS-%ld/blocksBeingWritten/blk_%ld", 
		curDir, r->header.namespace_id, r->header.block_id);
	
	r->path = string_xxxpdup(r->pool, (uchar_t *)tmpDir, strlen(tmpDir));
	if (!r->path) 
	{
        dfs_log_error(dfs_cycle->error_log, DFS_LOG_ERROR, errno, 
			"string_xxxpdup %s err", tmpDir);
		
	    return DFS_ERROR;
	}
	
    return DFS_OK;
}

int write_block_done(dn_request_t *r)
{
    char curDir[PATH_LEN] = "";
	char blkDir[PATH_LEN] = "";
	int  suddir_id = 0;
	int  suddir_id2 = 0;

	suddir_id = r->header.block_id % SUBDIR_LEN;
	suddir_id2 = (r->header.block_id % 1000) % SUBDIR_LEN;

	get_disk_id(r->header.block_id, curDir);
	sprintf(blkDir, "%s/NS-%ld/current/subdir%d/subdir%d/blk_%ld", 
		curDir, r->header.namespace_id, suddir_id, suddir_id2, 
		r->header.block_id);

	if (rename((char *)r->path, blkDir) != DFS_OK) 
	{
        dfs_log_error(dfs_cycle->error_log, DFS_LOG_ERROR, errno, 
			"rename %s to %s err", r->path, blkDir);
		
        return DFS_ERROR;
	}

	strcpy((char *)r->path, blkDir);
	    
    return recv_blk_report(r);
}

static int get_disk_id(long block_id, char *path)
{
    queue_t *head = NULL;
	queue_t *entry = NULL;
	int      disk_id = 0;

	disk_id = block_id % g_storage_dir_n;

	head = &g_storage_dir_q;
	entry = queue_next(head);

	while (head != entry) 
	{
        storage_dir_t *sd = queue_data(entry, storage_dir_t, me);

		entry = queue_next(entry);

		if (sd->id == disk_id) 
		{
		    strcpy(path, sd->current);
			
            break;
		}
	}

	return DFS_OK;
}

static int recv_blk_report(dn_request_t *r)
{
    block_info_t *blk = NULL;
		
    pthread_rwlock_wrlock(&g_dn_bcm->cache_rwlock);

	blk = (block_info_t *)mem_get0(g_dn_bcm->mem_mgmt.free_mblks);
	if (!blk)
	{
	    dfs_log_error(dfs_cycle->error_log, DFS_LOG_ALERT, 0, "mem_get0 err");
	}

	queue_init(&blk->me);
	
    blk->id = r->header.block_id;
	blk->size = r->header.len;
	strcpy(blk->path, (const char *)r->path);

	blk->ln.key = &blk->id;
    blk->ln.len = sizeof(blk->id);
    blk->ln.next = NULL;

	dfs_hashtable_join(g_dn_bcm->blk_htable, &blk->ln);

	pthread_rwlock_unlock(&g_dn_bcm->cache_rwlock);
	
    notify_nn_receivedblock(blk);
    
    return DFS_OK;
}

void *blk_scanner_start(void *arg)
{
	conf_server_t *sconf = NULL;
	int            blk_report_interval = 0;
	unsigned long  last_blk_report = 0;

	sconf = (conf_server_t *)dfs_cycle->sconf;
    blk_report_interval = sconf->block_report_interval;

	//struct timeval now;
	//gettimeofday(&now, NULL);
	//unsigned long diff = now.tv_sec * 1000 + now.tv_usec / 1000;

	//last_blk_report = diff;

	while (blk_scanner_running) 
	{
        //gettimeofday(&now, NULL);
	    //diff = (now.tv_sec * 1000 + now.tv_usec / 1000) - last_blk_report;

		//if (diff >= blk_report_interval) 
		//{
        //
		//}

		// scan dir
		queue_t *head = &g_storage_dir_q;
		queue_t *entry = queue_next(head);

		while (head != entry) 
		{
            storage_dir_t *sd = queue_data(entry, storage_dir_t, me);

			entry = queue_next(entry);

			scan_current_dir(sd->current);
		}

		sleep(blk_report_interval);
	}
	
    return NULL;
}

static int scan_current_dir(char *dir)
{
    char           root[PATH_LEN] = "";
	DIR           *p_dir = NULL;
	struct dirent *ent = NULL;
	
	p_dir = opendir(dir);
	if (NULL == p_dir) 
	{
	    dfs_log_error(dfs_cycle->error_log, DFS_LOG_ERROR, errno, 
			"opendir %s err", dir);
		
        return DFS_ERROR;
	}

	while (NULL != (ent = readdir(p_dir))) 
	{
        if (ent->d_type == 8) 
		{
	        // file
		}
		else if (0 == strncmp(ent->d_name, "NS-", 3)) 
		{
            char namespace_id[16] = "";
			get_namespace_id(ent->d_name, namespace_id);

            sprintf(root, "%s/%s/current", dir, ent->d_name);
			scan_namespace_dir(root, atol(namespace_id));
		}
	}

	closedir(p_dir);
	
    return DFS_OK;
}

static void get_namespace_id(char *src, char *id)
{
    char *pTemp = src + 3;

    while (*pTemp != '\0') 
	{
        *id++ = *pTemp++;
	}
}

static int scan_namespace_dir(char *dir, long namespace_id)
{
    char           root[PATH_LEN] = "";
	DIR           *p_dir = NULL;
	struct dirent *ent = NULL;
	
	p_dir = opendir(dir);
	if (NULL == p_dir) 
	{
	    dfs_log_error(dfs_cycle->error_log, DFS_LOG_ERROR, errno, 
			"opendir %s err", dir);
		
        return DFS_ERROR;
	}

	while (NULL != (ent = readdir(p_dir))) 
	{
        if (ent->d_type == 8) 
		{
	        // file
		}
		else if (0 == strncmp(ent->d_name, "subdir", 6)) 
		{
            sprintf(root, "%s/%s", dir, ent->d_name);
            scan_subdir(root, namespace_id);
		}
	}

	closedir(p_dir);
	
    return DFS_OK;
}

static int scan_subdir(char *dir, long namespace_id)
{
    char           root[PATH_LEN] = "";
	DIR           *p_dir = NULL;
	struct dirent *ent = NULL;
	
	p_dir = opendir(dir);
	if (NULL == p_dir) 
	{
	    dfs_log_error(dfs_cycle->error_log, DFS_LOG_ERROR, errno, 
			"opendir %s err", dir);
		
        return DFS_ERROR;
	}

	while (NULL != (ent = readdir(p_dir))) 
	{
        if (ent->d_type == 8) 
		{
	        // file
		}
		else if (0 == strncmp(ent->d_name, "subdir", 6)) 
		{
		    sprintf(root, "%s/%s", dir, ent->d_name);
            scan_subdir_subdir(root, namespace_id);
		}
	}

	closedir(p_dir);
	
    return DFS_OK;
}

static int scan_subdir_subdir(char *dir, long namespace_id)
{
    char           path[PATH_LEN] = "";
	DIR           *p_dir = NULL;
	struct dirent *ent = NULL;
	
	p_dir = opendir(dir);
	if (NULL == p_dir) 
	{
	    dfs_log_error(dfs_cycle->error_log, DFS_LOG_ERROR, errno, 
			"opendir %s err", dir);
		
        return DFS_ERROR;
	}

	while (NULL != (ent = readdir(p_dir))) 
	{
        if (8 == ent->d_type && 0 == strncmp(ent->d_name, "blk_", 4)) 
		{
	        char blk_id[16] = "";
			get_blk_id(ent->d_name, blk_id);

			sprintf(path, "%s/%s", dir, ent->d_name);
			block_object_add(path, namespace_id, atol(blk_id));
		}
	}

	closedir(p_dir);
	
    return DFS_OK;
}

static void get_blk_id(char *src, char *id)
{
    char *pTemp = src + 4;

    while (*pTemp != '\0') 
	{
        *id++ = *pTemp++;
	}
}

