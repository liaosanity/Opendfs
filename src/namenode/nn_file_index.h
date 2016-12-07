#ifndef NN_FILE_INDEX_H
#define NN_FILE_INDEX_H

#include "dfs_hashtable.h"
#include "dfs_mem_allocator.h"
#include "dfs_mblks.h"
#include "dfs_commpool.h"
#include "dfs_task.h"
#include "dfs_event.h"
#include "nn_cycle.h"
#include "nn_thread.h"

#define OP_INVALID        -1
#define OP_ADD             0
#define OP_RENAME          1
#define OP_RMR             2
#define OP_MKDIR           3
#define OP_SET_REPLICATION 4
#define OP_DATANODE_ADD    5
#define OP_DATANODE_REMOVE 6
#define OP_SET_PERMISSIONS 7
#define OP_SET_OWNER       8
#define OP_CLOSE           9
#define OP_SET_GENSTAMP    10
#define OP_SET_NS_QUOTA    11
#define OP_CLEAR_NS_QUOTA  12
#define OP_TIMES           13
#define OP_SET_QUOTA       14
#define OP_CONCAT_DELETE   16
#define OP_SYMLINK         17
#define OP_CRYPTO_ZONE     18
#define OP_CRYPTO_FILE     19

#define PATH_LEN  256
#define KEY_LEN   256
#define OWNER_LEN 16
#define GROUP_LEN 16
#define BLK_LIMIT 64

#define HASH_BUF_PER_SZ sizeof(void *) 
#define DFS_ALIGNMENT sizeof(uint64_t)
#define FI_STORE_SIZE (size_t)(dfs_align_ptr(sizeof(fi_store_t), DFS_ALIGNMENT))
#define FI_STORE_BUF_PER_SZ (FI_STORE_SIZE + sizeof(void *))

#define FI_POOL_REMAIN_MEM (10 * 1024)

#define FI_HASH_BUF(fi_count)  (fi_count * HASH_BUF_PER_SZ)
#define FI_STORE_BUF(fi_count) (fi_count * FI_STORE_BUF_PER_SZ)

#define FI_POOL_SIZE(fi_count) (FI_HASH_BUF(fi_count) \
        + FI_STORE_BUF(fi_count) + FI_POOL_REMAIN_MEM) 

typedef struct fi_inode_s
{
    char     key[KEY_LEN];
	uint64_t uid;
	short    permission;
	char     owner[OWNER_LEN];
	char     group[GROUP_LEN];
	uint64_t modification_time;
	uint64_t access_time;
	uint32_t is_directory:1;
	uint64_t length;
	uint64_t blk_size;
	short    blk_replication;
	uint64_t blks[BLK_LIMIT];
} fi_inode_t;

typedef struct fi_store_s 
{
	dfs_hashtable_link_t  ln;
	queue_t               ckp;
	queue_t 	          me;
	queue_t               children;
	uint64_t              children_num;
	fi_inode_t            fin;
	short	              state;
	dfs_thread_t	     *thread;
	event_t 		      timer_ev;
} fi_store_t;
        
typedef struct fi_cache_mem_s 
{
    void                *mem;
    size_t               mem_size;
    dfs_mem_allocator_t *allocator;
    struct mem_mblks    *free_mblks;
} fi_cache_mem_t;

typedef struct fi_cache_mgmt_s 
{
    dfs_hashtable_t  *fi_htable;
    pthread_rwlock_t  cache_rwlock;
    fi_cache_mem_t    mem_mgmt;
    dfs_hashtable_t  *fi_timer_htable;
    pthread_rwlock_t  timer_rwlock;
	int               timer_delay; // MSec
} fi_cache_mgmt_t;

int nn_file_index_worker_init(cycle_t *cycle);
int nn_file_index_worker_release(cycle_t *cycle);

int nn_mkdir(task_t *task);
int nn_rmr(task_t *task);
int nn_ls(task_t *task);
int nn_get_file_info(task_t *task);
int nn_create(task_t *task);
int nn_get_additional_blk(task_t *task);
int nn_close(task_t *task);
int nn_rm(task_t *task);
int nn_open(task_t *task);

int update_fi_cache_mgmt(const uint64_t llInstanceID, 
	const std::string & sPaxosValue, void *data); 

fi_store_t *get_store_obj(uchar_t *key);
void get_store_path(uchar_t *key, uchar_t *path);
int get_path_names(uchar_t *path, uchar_t names[][PATH_LEN]);
void key_encode(uchar_t *path, uchar_t *key);
void get_path_keys(uchar_t names[][PATH_LEN], int num, 
	uchar_t keys[][PATH_LEN]);
int get_path_inodes(uchar_t keys[][PATH_LEN], int num, 
	fi_inode_t *finodes[]);
int is_FsObjectExceed(int num);
int inc_FsObjectNum(int num);
int sub_FsObjectNum(int num);
int is_InSafeMode();

int do_checkpoint();
int load_image();

#endif

