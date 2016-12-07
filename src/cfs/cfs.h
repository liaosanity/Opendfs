#ifndef CFS_FS_H
#define CFS_FS_H

#include <dirent.h>
#include "dfs_lock.h"
#include "dfs_shmem.h"
#include "dfs_error_log.h"
#include "dfs_slabs.h"
#include "dfs_mem_allocator.h"
#include "dfs_hashtable.h"
#include "dfs_array.h"
#include "cfs_fio.h"

#define MAX_PATH 512 

typedef struct fs_meta_s fs_meta_t;
typedef struct swap_opt_s swap_opt_t;
typedef struct cfs_s cfs_t;

typedef void (*FSPARSE)(swap_opt_t *, fs_meta_t *);
typedef void (*FSSHUTDOWN)(void);
typedef void (*FSSETUP)(fs_meta_t *);

typedef void (*STOBJCLOSE)(int);
typedef int (*STOBJOPEN)(uchar_t *, int, log_t *);
typedef int (*STOBJREAD)(file_io_t *, log_t *);
typedef int (*STOBJWRITE)(file_io_t *, log_t *);
typedef int (*STOPTSENDFILE)(int, int, off_t* , size_t, log_t *);
typedef int (*STOPTSENDFILECHAIN)(file_io_t *, log_t *);
typedef int (*STOBJINIT)(int);

typedef int (*STLOGOPEN)(uchar_t *, int, log_t *);
typedef void (*STLOGCLOSE)(int);

typedef struct fs_meta_s 
{
	FSPARSE    parsefunc;
	FSSHUTDOWN donefunc;
} fs_meta_t;

typedef struct swap_opt_s 
{         
    struct 
	{
        STOBJCLOSE         close;
        STOBJOPEN          open;
        STOBJREAD          read;
        STOBJWRITE         write;
    	STOPTSENDFILE      sendfile;
    	STOPTSENDFILECHAIN sendfilechain;
        STOBJINIT          ioinit;
    } io_opt;
	
    struct 
	{
        STLOGOPEN open;
        STLOGCLOSE close;
    } log_opt;
} swap_opt_t;

typedef struct cfs_s 
{
	fs_meta_t          *meta;
	swap_opt_t	       *sp;
	volatile uint64_t  *cursize;
	dfs_slab_manager_t *slab_mgr;            
    int                 state;
} cfs_t;

typedef struct io_event_s 
{
    volatile queue_t  posted_events;
    dfs_atomic_lock_t lock;
    volatile queue_t  posted_bad_events;
    dfs_atomic_lock_t bad_lock;
} io_event_t;

typedef struct sendfile_chain_task_s 
{
    void     *buf_chain;
    int       conn_fd;
    int       store_fd;
    uint64_t  limit;
    uint64_t  sent;
    void     *file_io;
} sendfile_chain_task_t;

int  cfs_setup(pool_t *, cfs_t *, log_t *);
int  cfs_open(cfs_t *, uchar_t *, int, log_t *);
void cfs_close(cfs_t *, int);
int  cfs_read(cfs_t *, file_io_t *, log_t *);
int  cfs_write(cfs_t *, file_io_t *, log_t *);
int  cfs_sendfile(cfs_t *, int, int, off_t *, size_t, log_t *);
int  cfs_sendfile_chain(cfs_t *, file_io_t *, log_t *);
int  cfs_size_add(volatile uint64_t *, uint64_t);
int  cfs_size_sub(volatile uint64_t *, uint64_t, log_t *);
int  cfs_prepare_work(cycle_t *cycle);
int  cfs_ioevent_init(io_event_t *io_event);
void cfs_ioevents_process_posted(io_event_t *, fio_manager_t *);
int  cfs_notifier_init(faio_notifier_manager_t *faio_notify);
void cfs_recv_event(faio_notifier_manager_t  *faio_notify);

#endif

