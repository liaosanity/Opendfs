#ifndef CFS_FIO_H
#define CFS_FIO_H

#include "dfs_types.h"
#include "dfs_queue.h"
#include "dfs_memory_pool.h"
#include "dfs_buffer.h"
#include "dfs_string.h"
#include "faio_manager.h"
#include "dn_cycle.h"

#define AIO_READ_EV  0
#define AIO_WRITE_EV 1

#define AIO_NOTFIN   0
#define AIO_FINAL    1

#define AIO_FAILED   0
#define AIO_OK       1
#define AIO_PENDING  2

#define AIO_ABLE     0
#define AIO_DISABLE  1

#define FIO_MANAGER_NUM_DEF 	4096
#define FIO_MANAGER_MEM_EDGE  	0.95
#define AIO_NUM_STEP			128
#define AIO_BUF_MAX_DEF    	 	1048576
#define AIO_MAX_TASK_NUM		3

#define AIO_MGR_OPEN   1
#define AIO_MGR_CLOSED 0

#define MAX_TASK_IDLE  32

enum 
{
    TASK_STORE_HEADER,
    TASK_STORE_BODY
};

typedef int (*file_io_handler_pt) (void *, void *);

typedef struct file_s
{
    int fd;
    string_t name;
    struct stat info;
    off_t offset;
    off_t sys_offset;
    size_t size;
    log_t *log;
    pool_t *pool;
    uint32_t valid_info;
} file_t;

typedef struct file_io_s 
{
    off_t	                 offset;
	uint32_t	             need;
    int	                     fd;
    buffer_t                *b;
    void                    *data;
    file_io_handler_pt       h;
    int64_t                  ret;
	int	                     event:2;
	int	                     index:2;
	int	                     result:2;
	int                      able:2;
    int                      type;
    int                      error;
    queue_t                  q;
    queue_t                  used;
    void                    *io_event;
    faio_notifier_manager_t *faio_noty;
    faio_data_task_t         faio_task;
    int                      faio_ret;
    void                    *sf_chain_task;
} file_io_t;

typedef struct fio_manager_s 
{
    queue_t         freeq;
    uint32_t        idle;
    uint32_t        batch;
    uint64_t        free;
    uint64_t        busy;
    uint64_t        nelts;
    uint64_t        max;
    uint32_t        threads;
    uint32_t        size;
    pthread_mutex_t lock;
    queue_t         task_used;
    uint64_t        refill_level;
} fio_manager_t;

int cfs_fio_manager_init(cycle_t *cycle, fio_manager_t *fio_manager);
int cfs_fio_manager_destroy(fio_manager_t *fio_manager);
int cfs_fio_manager_free(file_io_t *dst, fio_manager_t *fio_manager);
file_io_t *cfs_fio_manager_alloc(fio_manager_t *fio_manager);

#endif


