#ifndef DFS_CLI_CYCLE_H
#define DFS_CLI_CYCLE_H

#include "dfs_types.h"
#include "dfs_string.h"
#include "dfs_array.h"
#include "dfs_memory_pool.h"

typedef struct cycle_s 
{
    void      *sconf;
    pool_t    *pool;
    log_t     *error_log;
    string_t   conf_file;
} cycle_t;

extern cycle_t *dfs_cycle;

cycle_t  *cycle_create();
int       cycle_init(cycle_t *cycle);
int       cycle_free(cycle_t *cycle);

#endif

