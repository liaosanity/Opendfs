#include "dfscli_cycle.h"
#include "dfs_conf.h"
#include "dfs_memory.h"
#include "dfscli_conf.h"

#define CYCLE_POOL_SIZE 16384

cycle_t         *dfs_cycle;  
extern string_t  config_file;

cycle_t *cycle_create()
{
    cycle_t *cycle = NULL;
    cycle = (cycle_t *)memory_calloc(sizeof(cycle_t));
    
    if (!cycle) 
	{
        printf("create cycle faild!\n");
		
        return cycle;
    }
    
    cycle->pool = pool_create(CYCLE_POOL_SIZE, CYCLE_POOL_SIZE, NULL);
    if (!cycle->pool) 
	{
        memory_free(cycle, sizeof(cycle_t));
        cycle = NULL;
    }
    
    return cycle;
}

int cycle_init(cycle_t *cycle)
{
    log_t          *log = NULL;
    conf_context_t *ctx = NULL;
    conf_object_t  *conf_objects = NULL;
    string_t        server = string_make("Server");
    
    if (cycle == NULL) 
	{
        return DFS_ERROR;
    }
  
    cycle->conf_file.data = string_xxpdup(cycle->pool , &config_file);
    cycle->conf_file.len = config_file.len;
    
    if (!dfs_cycle) 
	{
        log = error_log_init_with_stderr(cycle->pool);
        if (!log) 
		{
            goto error;
        }
		
        cycle->error_log = log;
        cycle->pool->log = log;
        dfs_cycle = cycle;
    }
	
    error_log_set_handle(log, NULL, NULL);
 
    ctx = conf_context_create(cycle->pool);
    if (!ctx) 
	{
        goto error;
    }
    
    conf_objects = get_dn_conf_object();
    
    if (conf_context_init(ctx, &config_file, log, conf_objects) != DFS_OK) 
	{
        goto error;
    }
    
    if (conf_context_parse(ctx) != DFS_OK) 
	{
        printf("configure parse failed at line %d\n", ctx->conf_line); 
		
        goto error;
    }
    
    cycle->sconf = conf_get_parsed_obj(ctx, &server);
    if (!cycle->sconf) 
	{
        printf("no Server conf\n");
		
        goto error;
    }
	
    return DFS_OK;
    
error:
     return DFS_ERROR;
}

int cycle_free(cycle_t *cycle)
{
    if (!cycle) 
	{
        return DFS_OK;
    }
   
    if (cycle->pool) 
	{
        pool_destroy(cycle->pool);
    }
    
    memory_free(cycle, sizeof(cycle_t));

    cycle = NULL;

    return DFS_OK;
}

