#ifndef DFS_ERROR_LOG_H
#define DFS_ERROR_LOG_H

#include "dfs_types.h"
#include "dfs_string.h"

#define DFS_MAX_ERROR_STR 2048

enum 
{
    DFS_LOG_FATAL = 1,
    DFS_LOG_ERROR,
    DFS_LOG_ALERT,
    DFS_LOG_WARN,
    DFS_LOG_NOTICE,
    DFS_LOG_INFO,
    DFS_LOG_CRIT,
    DFS_LOG_EMERG,
    DFS_LOG_DEBUG,
};

typedef string_t* (*log_time_ptr)(void);
typedef string_t* (*log_level_ptr)(int level);

typedef struct log_flog_str_s 
{
    string_t  log_path;
    string_t  daemon_log;
    string_t  local_log;
    string_t  dfslog_path;
    int       log_style_type;
} log_flog_str_t;

typedef struct open_file_s 
{
    int      fd;
    string_t name;
} open_file_t;

struct log_s 
{
    int           log_type;
    uint32_t      log_level;
    open_file_t   *file;
    log_flog_str_t log_flog;
    log_time_ptr   log_time_handler;
    log_level_ptr  log_level_handler;
};

enum 
{
    LOG_NORMAL = 1,
    LOG_DFSLOG,
};

log_t *error_log_init_with_stderr(pool_t *pool);
int    error_log_init(log_t *log, log_time_ptr tm_handler, 
	log_level_ptr lv_handler);
int    error_log_release(log_t *log);
void   error_log_core (log_t *log, uint32_t level, 
    char *file, int line, int err, const char *fmt, ...);
void   error_log_debug_core(log_t *log, uint32_t level, 
    char *file, int line, int err, const char *fmt, ...);
log_t *error_log_open(pool_t *pool, string_t name);
int    error_log_close(log_t *log);
int error_log_set_handle(log_t *log, log_time_ptr tm_handler, 
	log_level_ptr lv_handler);

#endif

