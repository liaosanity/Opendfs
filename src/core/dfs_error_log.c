#include <string.h>

#include "dfs_error_log.h"
#include "dfs_memory.h"
#include "dfs_memory_pool.h"
#include "dfs_ipc.h"

static uchar_t *error_log_strerror(int err, uchar_t *errstr, size_t size);

static string_t error_levels[] = 
{
    string_null,
    string_make("fatal"),
    string_make("error"),
    string_make("alert"),
    string_make("warn"),
    string_make("notice"),
    string_make("info"),
    string_make("crit"),
    string_make("emerg"),
    string_make("debug")
};

static string_t * default_level(int level)
{
    return &error_levels[DFS_LOG_DEBUG];
}

log_t * error_log_init_with_stderr(pool_t *pool)
{
    log_t *log = (log_t *)pool_alloc(pool, sizeof(log_t));
    if (!log) 
	{
        return NULL;
    }
	
    log->log_level = DFS_LOG_NOTICE;
    log->file = (open_file_t *)pool_alloc(pool, sizeof(open_file_t));
    if (!log->file) 
	{
        return NULL;
    }
	
    memory_zero(log->file, sizeof(open_file_t));
    log->file->fd = STDERR_FILENO;

    return log;
}

int error_log_init(log_t  *slog, log_time_ptr tm_handler, 
	                  log_level_ptr lv_handler)
{
    if (*(slog->file->name.data) == '|') 
	{
        slog->file->fd = dfs_ipc_open(&slog->file->name);
    } 
	else 
	{
        slog->file->fd = dfs_sys_open(slog->file->name.data,
            O_APPEND | O_WRONLY | O_CREAT, 0644);
    }

    if (slog->file->fd < 0) 
	{
        printf("error_log_init_with_sconf: open error log failed!");
		
        return DFS_ERROR;
    }
	
    slog->log_time_handler = tm_handler;
    slog->log_level_handler = !lv_handler ? default_level : lv_handler;
	
    return DFS_OK;
}

int error_log_release(log_t  *slog)
{  
    if (slog && slog->file->fd >= 0) 
	{
        error_log_close(slog);
    }
	
    return DFS_OK;
}

void error_log_core(log_t *log, uint32_t level, char *file, 
	                     int line, int err, const char *fmt, ...)
{
    va_list   args;
    uchar_t  *p = NULL;
    uchar_t  *last = NULL;
    ssize_t   n = 0;
    ssize_t   logsz = 0;
    uchar_t   errstr[DFS_MAX_ERROR_STR];
    int       len = 0;
    string_t *stime;
	
 	if (level > log->log_level) 
	{
		return;
 	}
	
    if (!log || log->file->fd == DFS_INVALID_FILE) 
	{
        return;
    }
	
    if (log->log_type == LOG_DFSLOG) 
	{
        len = 1;
        errstr[0] = 'L';
    }
	
    stime = log->log_time_handler();
    last = errstr + DFS_MAX_ERROR_STR;
    memory_memcpy(errstr + len, stime->data, stime->len);
    p = errstr + len + stime->len;
    p = string_xxsnprintf(p, last - p, " [%s] ", error_levels[level].data);
    p = string_xxsnprintf(p, last - p, "%P#" DFS_TID_T_FMT ": ",
        getpid(),  pthread_self());
    p = string_xxsnprintf(p, last - p, "[%s:%d] ", file, line);
    va_start(args, fmt);
    p = string_xxvsnprintf(p, last - p, fmt, args);
    va_end(args);

    if (err) 
	{
        if (p > last - 50) 
		{
            p = last - 50;
            *p++ = '.';
            *p++ = '.';
            *p++ = '.';
        }

        p = string_xxsnprintf(p, last - p, " (%d: ", err);
        p = error_log_strerror(err, p, last - p);
        if (p < last) 
		{
            *p++ = ')';
        }
    }

    if (p > last - DFS_LINEFEED_SIZE) 
	{
        p = last - DFS_LINEFEED_SIZE;
    }

    *p++ = LF;
	
    if (log->file->fd > 0) 
	{
        logsz = p - errstr;
		
        n = dfs_write_fd(log->file->fd, errstr, logsz);
        if (n < 0) 
		{
            error_log_close(log);
        }
    }
}

void error_log_debug_core(log_t *log, uint32_t level, char *file, 
	                              int line, int err, const char *fmt, ...)
{
    va_list   args;
    ssize_t   n = 0;
    ssize_t   logsz = 0;
    uchar_t  *p = NULL;
    uchar_t  *last = NULL;
    uchar_t   errstr[DFS_MAX_ERROR_STR];
    int       len = 0;
    string_t *stime = NULL;
    string_t *slevel = NULL;

    if (!log || !(level & log->log_level)
        || (log->file->fd == DFS_INVALID_FILE)) 
    {
        return;
    }
	
	if (level > log->log_level) 
	{
		return;
 	}
	
    if (log->log_type ==LOG_DFSLOG) 
	{
        len = 1;
        errstr[0] = 'L';
    }

    stime = log->log_time_handler();
    last = errstr + DFS_MAX_ERROR_STR;
    memory_memcpy(errstr + len, stime->data, stime->len);
    p = errstr + stime->len + len;
    slevel = log->log_level_handler(level);
    p = string_xxsnprintf(p, last - p, " [%s] ", slevel->data);
    p = string_xxsnprintf(p, last - p, "%P#" DFS_TID_T_FMT ": ",
        getpid(),  pthread_self());
    p = string_xxsnprintf(p, last - p, "[%s:%d] ", file, line);
    va_start(args, fmt);
    p = string_xxvsnprintf(p, last - p, fmt, args);
    va_end(args);
  
    if (err) 
	{
        if (p > last - 50) 
		{
            p = last - 50;
            *p++ = '.';
            *p++ = '.';
            *p++ = '.';
        }

        p = string_xxsnprintf(p, last - p, " (%d: ", err);
        p = error_log_strerror(err, p, last - p);
		
        if (p < last) 
		{
            *p++ = ')';
        }
    }

    if (p > last - DFS_LINEFEED_SIZE) 
	{
        p = last - DFS_LINEFEED_SIZE;
    }

    *p++ = LF;
	
    if (log->file->fd > 0) 
	{
        logsz = p - errstr;
		
        n = dfs_write_fd(log->file->fd, errstr, logsz);
        if (n < 0) 
		{
            error_log_close(log);
        }
    }
}

static uchar_t * error_log_strerror(int err, uchar_t *errstr, size_t size)
{
    char *str = NULL;

    if (size == 0 || !errstr) 
	{
        return 0;
    }
    
    errstr[0] = '\0';
	
    str = strerror_r(err, (char *) errstr, size);
    if (str != (char *) errstr)
	{
        string_strncpy(errstr, (uchar_t *) str, size);
    }

    while (*errstr && size) 
	{
        errstr++;
        size--;
    }

    return errstr;
}

log_t * error_log_open(pool_t *pool, string_t name)
{
    log_t *log = NULL;

    if (!pool || name.len == 0) 
	{
        return log;
    }
	
    log = (log_t *)pool_alloc(pool, sizeof(log_t));
    if (!log) 
	{
        return log;
    }
	
    log->file = (open_file_t *)pool_calloc(pool, sizeof(open_file_t));
    if (!log->file) 
	{
        return log;
    }
	
    log->file->name = name;

    if (*(log->file->name.data) == '|') 
	{
        log->file->fd = dfs_ipc_open(&log->file->name);
    } 
	else 
	{
        log->file->fd = dfs_sys_open(log->file->name.data,
            O_APPEND | O_WRONLY | O_CREAT, 0644);
    }

    if (log->file->fd < 0) 
	{
        return NULL;
    }

    return log;
}

int error_log_close(log_t *log)
{
    if (log && log->file && log->file->fd >= 0)
	{
        close(log->file->fd);
        log->file->fd = DFS_INVALID_FILE;
    }

    return DFS_OK;
}

int error_log_set_handle(log_t *log, log_time_ptr tm_handler, 
	                            log_level_ptr lv_handler)
{
    log->log_time_handler = tm_handler;
    log->log_level_handler = !lv_handler ? default_level : lv_handler;
	
    return DFS_OK;
}

