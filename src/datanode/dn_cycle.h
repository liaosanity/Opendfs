#ifndef DN_CYCLE_H
#define DN_CYCLE_H

#include "dfs_types.h"
#include "dfs_string.h"
#include "dfs_array.h"
#include "dfs_memory_pool.h"

typedef struct cycle_s 
{
    void      *sconf;
    pool_t    *pool;
    log_t     *error_log;
    array_t    listening_for_cli;
	char       listening_ip[32];
    string_t   conf_file;
	void      *cfs;
} cycle_t;

extern cycle_t *dfs_cycle;

cycle_t  *cycle_create();
int       cycle_init(cycle_t *cycle);
int       cycle_free(cycle_t *cycle);
array_t  *cycle_get_listen_for_cli();
int       cycle_check_sys_env(cycle_t *cycle);

#endif

