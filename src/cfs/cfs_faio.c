#include <stdio.h>
#include <unistd.h>   
#include <fcntl.h>    
#include <string.h>   
#include <inttypes.h> 
#include <stdlib.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/sendfile.h>
#include <sys/mman.h>
#include <errno.h>
#include <error.h>
#include <time.h>
#include <stddef.h>
#include "dfs_shmem.h"
#include "dfs_error_log.h"
#include "dfs_slabs.h"
#include "dfs_mem_allocator.h"
#include "dfs_hashtable.h"
#include "dfs_array.h"
#include "dfs_conn.h"
#include "cfs.h"
#include "cfs_faio.h"

#define DFS_SENDFILE_LIMIT 2147479552L

faio_manager_t *faio_mgr;

typedef struct chain_s 
{
    buffer_t *buf;
    chain_t  *next;
} chain_t;

static int cfs_faio_ioinit(int thread_num);
static int cfs_faio_read(file_io_t *data, log_t *log);
static int cfs_faio_write(file_io_t *data, log_t *log);
static int cfs_faio_sendfile(file_io_t *data, log_t *log);
static int cfs_faio_open(uchar_t *path, int flags, log_t *log);
static void cfs_faio_close(int fd);
static void cfs_faio_parse(swap_opt_t *sp, fs_meta_t *meta);
static void cfs_faio_done(void);

static int cfs_faio_ioinit(int thread_num)
{
    faio_errno_t      error;
    faio_properties_t property;
    
    memset(&error, 0x00, sizeof(error));

    property.idle_timeout = 5;
    property.max_idle = 2;
    property.max_thread = thread_num;
    property.pre_start = 2;

    faio_mgr = (faio_manager_t *)malloc(sizeof(faio_manager_t));

    if (faio_manager_init(faio_mgr, &property, 0, &error) != FAIO_OK) 
	{
        return DFS_ERROR;
    }

    if (faio_register_handler(faio_mgr, cfs_faio_io_read, FAIO_IO_TYPE_READ, 
        &error) != FAIO_OK) 
    {
        goto faio_mgr_release;
    }

    if (faio_register_handler(faio_mgr, cfs_faio_io_write, FAIO_IO_TYPE_WRITE, 
        &error) != FAIO_OK) 
    {
        goto faio_mgr_release;
    }

    if (faio_register_handler(faio_mgr, cfs_faio_io_send_file, 
        FAIO_IO_TYPE_SENDFILE, &error) != FAIO_OK) 
    {
        goto faio_mgr_release;
    }

    return DFS_OK;

faio_mgr_release:
    faio_manager_release(faio_mgr, &error);

    return DFS_ERROR;
}

static int cfs_faio_read(file_io_t *data, log_t *log)
{
    faio_errno_t             error;
    faio_notifier_manager_t *faio_noty = NULL;

    faio_noty = data->faio_noty;
    
    if (faio_read(faio_noty, cfs_faio_read_callback, &data->faio_task, &error) 
        != FAIO_OK) 
    {
        return DFS_ERROR;
    }

    return DFS_OK;
}

static int cfs_faio_write(file_io_t *data, log_t *log)
{
    faio_errno_t             error;
    faio_notifier_manager_t *faio_noty = NULL;

    faio_noty = data->faio_noty;

    if (faio_write(faio_noty, cfs_faio_write_callback, &data->faio_task, &error) 
        != FAIO_OK) 
    {
        return DFS_ERROR;
    }

    return DFS_OK;
}

static int cfs_faio_sendfile(file_io_t *data, log_t *log)
{
	faio_errno_t             error;
    faio_notifier_manager_t *faio_noty = NULL;

    faio_noty = data->faio_noty;

    if (faio_sendfile(faio_noty, cfs_faio_send_file_callback, &data->faio_task, 
        &error) != FAIO_OK) 
    {
        return DFS_ERROR;
    }

    return DFS_OK;
}

static int cfs_faio_open(uchar_t *path, int flags, log_t *log)
{
    int fd = DFS_INVALID_FILE;

    fd = dfs_sys_open(path, flags, 0400);
    if (fd < 0) 
	{
        return fd;
    }

    return fd;
}

static void cfs_faio_close(int fd)
{
    close(fd);    
}

void cfs_faio_write_callback(faio_data_task_t *task)
{
    cfs_faio_read_callback(task);
}

void cfs_faio_read_callback(faio_data_task_t *task)
{
    file_io_t        *file_io = NULL;
    dfs_lock_errno_t  error;
    int               res = 0;

    file_io = (file_io_t *)((char *)task - offsetof(file_io_t, faio_task));
    res = file_io->faio_ret;
    
    if (res < 0) 
	{
        file_io->result = AIO_FAILED;
    } 
	else 
	{
        file_io->result = AIO_OK;
		
        if (file_io->event == AIO_READ_EV) 
		{
            file_io->b->last += res;
            file_io->ret = res;
        }
    }

    file_io->error = file_io->ret = res;

    if (file_io->result == AIO_FAILED) 
	{
        dfs_atomic_lock_on(&((io_event_t *)file_io->io_event)->bad_lock, 
            &error);

        queue_insert_tail((queue_t *)&(
            ((io_event_t *)file_io->io_event)->posted_bad_events),
            &file_io->q);

        dfs_atomic_lock_off(&((io_event_t *)file_io->io_event)->bad_lock, 
            &error);
    } 
	else if (file_io->result == AIO_OK)
	{
        dfs_atomic_lock_on(&((io_event_t *)file_io->io_event)->lock, &error);

        queue_insert_tail((queue_t *)&(
            ((io_event_t *)file_io->io_event)->posted_events),
            &file_io->q);

        dfs_atomic_lock_off(&((io_event_t *)file_io->io_event)->lock, &error);
    }
}

int cfs_faio_io_read(faio_data_task_t *task)
{
    file_io_t *file_task = NULL;
    int        ret = 0;

    file_task = (file_io_t *)((char *)task - offsetof(file_io_t, faio_task));

    if ((ret = pread(file_task->fd, file_task->b->last, file_task->need, 
        file_task->offset)) < 0) 
    {
        task->err.sys = errno;
		
        return DFS_ERROR;
    }

    file_task->faio_ret = ret;

    return DFS_OK;
}

int cfs_faio_io_write(faio_data_task_t *task)
{
    file_io_t *file_task = NULL;
    int        ret = 0;

    file_task = (file_io_t *)((char *)task - offsetof(file_io_t, faio_task));

    if ((ret = pwrite(file_task->fd, file_task->b->start, 
        file_task->b->last - file_task->b->start, file_task->offset)) < 0) 
    {
        task->err.sys = errno;
		
        return DFS_ERROR;
    }

    file_task->faio_ret = ret;

    return DFS_OK;
        
}

int cfs_faio_io_send_file(faio_data_task_t *task)
{
    file_io_t             *file_task = NULL;
    int                    rc = 0;
    size_t                 limit = 0;
    sendfile_chain_task_t *sf_chain_task = NULL;

    file_task = (file_io_t *)((char *)task - offsetof(file_io_t, faio_task));    

    sf_chain_task = (sendfile_chain_task_t *)file_task->sf_chain_task;
    limit = sf_chain_task->limit;
    
    if (limit == 0 || limit > DFS_SENDFILE_LIMIT) 
	{
        limit = DFS_SENDFILE_LIMIT;
    }

    while (file_task->need > 0)
	{
		rc = sendfile(sf_chain_task->conn_fd, sf_chain_task->store_fd, 
			&file_task->offset, file_task->need);
        
        if (rc == DFS_ERROR) 
		{
            if (errno == DFS_EAGAIN) 
			{
                file_task->faio_ret = DFS_EAGAIN;
				
                return DFS_OK;
            } 
			else if (errno == DFS_EINTR) 
			{
                continue;
            }
			
            file_task->faio_ret = DFS_ERROR;
			
            return DFS_ERROR;
        }

        if (!rc) 
		{
            file_task->faio_ret = DFS_ERROR;
			
            return DFS_ERROR;
        }

        if (rc > 0) 
		{
			file_task->need -= rc;
        }
    }

    file_task->faio_ret = DFS_OK;

    return DFS_OK;
}

void cfs_faio_send_file_callback(faio_data_task_t *task)
{
    cfs_faio_read_callback(task);
}

static void cfs_faio_parse(swap_opt_t *sp, fs_meta_t *meta)
{
    sp->io_opt.ioinit = cfs_faio_ioinit;
    sp->io_opt.read = cfs_faio_read;
    sp->io_opt.write = cfs_faio_write;
    sp->io_opt.open = cfs_faio_open;
    sp->io_opt.close = cfs_faio_close;
    sp->io_opt.sendfilechain = cfs_faio_sendfile;
}

static void cfs_faio_done(void)
{
}

void cfs_faio_setup(fs_meta_t *meta)
{
    meta->parsefunc = cfs_faio_parse;
    meta->donefunc = cfs_faio_done;
}


