#include "nn_module.h"
#include "nn_error_log.h"
#include "nn_rpc_server.h"
#include "nn_paxos.h"
#include "nn_file_index.h"
#include "nn_dn_index.h"
#include "nn_blk_index.h"

static int dfs_mod_max = 0;

dfs_module_t dfs_modules[] = 
{
    // please miss the beginning;
    {
        string_make("errlog"),
        0,
        PROCESS_MOD_INIT,
        NULL,
        nn_error_log_init,
        nn_error_log_release,
        NULL,
        NULL,
        NULL,
        NULL
    },

    {
        string_make("paxos"),
        0,
        PROCESS_MOD_INIT,
        NULL,
        NULL,
        NULL,
        nn_paxos_worker_init,
        nn_paxos_worker_release,
        NULL,
        NULL
    },

    {
        string_make("rpc_server"),
        0,
        PROCESS_MOD_INIT,
        NULL,
        NULL,
        NULL,
        nn_rpc_worker_init,
        nn_rpc_worker_release,
        NULL,
        NULL
    },

    {
        string_make("file_index"),
        0,
        PROCESS_MOD_INIT,
        NULL,
        NULL,
        NULL,
        nn_file_index_worker_init,
        nn_file_index_worker_release,
        NULL,
        NULL
    },

	{
        string_make("dn_index"),
        0,
        PROCESS_MOD_INIT,
        NULL,
        NULL,
        NULL,
        nn_dn_index_worker_init,
        nn_dn_index_worker_release,
        NULL,
        NULL
    },

	{
        string_make("blk_index"),
        0,
        PROCESS_MOD_INIT,
        NULL,
        NULL,
        NULL,
        nn_blk_index_worker_init,
        nn_blk_index_worker_release,
        NULL,
        NULL
    },

    {string_null, 0, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL}
};

void dfs_module_setup(void)
{
    int i = 0;
	
    for (i = 0; dfs_modules[i].name.data != NULL; i++) 
	{
        dfs_modules[i].index = dfs_mod_max++;
    }
}

int dfs_module_master_init(cycle_t *cycle)
{
    int i = 0;
	
    for (i = 0; i < dfs_mod_max; i++) 
	{
        if (dfs_modules[i].master_init != NULL &&
            dfs_modules[i].master_init(cycle) == DFS_ERROR) 
        {
            printf("process_master_init: module %s init failed\n",
                dfs_modules[i].name.data);
			
            return DFS_ERROR;
        }

        dfs_modules[i].flag = PROCESS_MOD_FREE;
    }
	
    return DFS_OK;
}

int dfs_module_master_release(cycle_t *cycle)
{
    int i = 0;
	
    for (i = dfs_mod_max - 1; i >= 0; i--) 
	{
        if (dfs_modules[i].flag != PROCESS_MOD_FREE  ||
            dfs_modules[i].master_release == NULL) 
        {
            continue;
        }
		
        if (dfs_modules[i].master_release(cycle) == DFS_ERROR) 
		{
            dfs_log_error(cycle->error_log, DFS_LOG_ERROR, 0,
                "%s deinit fail \n",dfs_modules[i].name.data);
			
            return DFS_ERROR;
        }
		
        dfs_modules[i].flag = PROCESS_MOD_INIT;
    }
	
    return DFS_OK;
}

int dfs_module_woker_init(cycle_t *cycle)
{
    int i = 0;
	
    for (i = 0; i < dfs_mod_max; i++) 
	{
        if (dfs_modules[i].worker_init != NULL &&
            dfs_modules[i].worker_init(cycle) == DFS_ERROR) 
        {
            dfs_log_error(cycle->error_log, DFS_LOG_ERROR, 0,
                "dfs_module_init_woker: module %s init failed\n",
                dfs_modules[i].name.data);
			
            return DFS_ERROR;
        }
    }
	
    return DFS_OK;
}

int dfs_module_woker_release(cycle_t *cycle)
{
    int i = 0;
	
    for (i = 0; i < dfs_mod_max; i++) 
	{
        if (dfs_modules[i].worker_release!= NULL &&
            dfs_modules[i].worker_release(cycle) == DFS_ERROR) 
        {
            dfs_log_error(cycle->error_log, DFS_LOG_ERROR, 0,
                "dfs_module_init_woker: module %s init failed\n",
                dfs_modules[i].name.data);
			
            return DFS_ERROR;
        }
    }
	
    return DFS_OK;
}

int dfs_module_workethread_init(dfs_thread_t *thread)
{
    int i = 0;
	
    for (i = 0; i < dfs_mod_max; i++) 
	{
        if (dfs_modules[i].worker_thread_init != NULL &&
            dfs_modules[i].worker_thread_init(thread) == DFS_ERROR) 
        {
            dfs_log_error(dfs_cycle->error_log, DFS_LOG_ERROR, 0,
                "dfs_module_init_woker: module %s init failed\n",
                dfs_modules[i].name.data);
			
            return DFS_ERROR;
        }
    }
	
    return DFS_OK;
}

int dfs_module_wokerthread_release(dfs_thread_t *thread)
{
    int i = 0;

    for (i = 0; i < dfs_mod_max; i++) 
	{
        if (dfs_modules[i].worker_thread_release != NULL &&
            dfs_modules[i].worker_thread_release(thread) == DFS_ERROR) 
        {
            dfs_log_error(dfs_cycle->error_log, DFS_LOG_ERROR, 0,
                "dfs_module_init_woker: module %s init failed\n",
                dfs_modules[i].name.data);
			
            return DFS_ERROR;
        }
    }

    return DFS_OK;
}

