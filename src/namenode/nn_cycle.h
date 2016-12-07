#ifndef NN_CYCLE_H
#define NN_CYCLE_H

#include "dfs_types.h"
#include "dfs_string.h"
#include "dfs_array.h"
#include "dfs_memory_pool.h"
#include "FSEditlog.h"

typedef struct cycle_s cycle_t;

struct cycle_s 
{
    void      *sconf;
    pool_t    *pool;
    log_t     *error_log;
    array_t    listening_for_cli;
    array_t    listening_for_dn;
    string_t   conf_file;
    string_t   admin;
	uint64_t   namespace_id;
};

extern cycle_t *dfs_cycle;

cycle_t  *cycle_create();
int       cycle_init(cycle_t *cycle);
int       cycle_free(cycle_t *cycle);
array_t  *cycle_get_listen_for_cli();
array_t  *cycle_get_listen_for_dn();
int       cycle_check_sys_env(cycle_t *cycle);

#endif

