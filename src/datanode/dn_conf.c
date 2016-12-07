#include "dfs_memory_pool.h"
#include "config.h"
#include "dfs_array.h"
#include "dn_cycle.h"
#include "dn_conf.h"

#define ALLOW    1
#define DENY     2

#define CONF_ON  1
#define CONF_OFF 0

#define CONF_SERVER_BIND_N 1 

extern cycle_t *dfs_cycle;

static void *conf_server_init(pool_t *pool);
static int   conf_server_make_default(void *var);

static int conf_parse_nn_macro(conf_variable_t *v, uint32_t offset,
    int type, string_t *args, int args_n);

static conf_option_t conf_server_option[] = 
{
    { string_make("daemon"), conf_parse_nn_macro,
        OPE_EQUAL, offsetof(conf_server_t, daemon) },
        
    { string_make("workers"), conf_parse_int,
        OPE_EQUAL, offsetof(conf_server_t, worker_n) },
        
    { string_make("bind_for_cli"), conf_parse_bind,
        OPE_EQUAL, offsetof(conf_server_t, bind_for_cli) },

	{ string_make("ns_srv"), conf_parse_string,
        OPE_EQUAL, offsetof(conf_server_t, ns_srv) },
        
    { string_make("connections"), conf_parse_int,
        OPE_EQUAL, offsetof(conf_server_t, connection_n) },
        
    { string_make("error_log"), conf_parse_string,
        OPE_EQUAL, offsetof(conf_server_t, error_log) },
        
    { string_make("coredump_dir"), conf_parse_string,
        OPE_EQUAL, offsetof(conf_server_t, coredump_dir) },
        
    { string_make("pid_file"), conf_parse_string,
        OPE_EQUAL, offsetof(conf_server_t, pid_file) },
        
    { string_make("log_level"), conf_parse_nn_macro,
        OPE_EQUAL, offsetof(conf_server_t, log_level) },
        
    { string_make("recv_buff_len"), conf_parse_bytes_size,
        OPE_EQUAL, offsetof(conf_server_t, recv_buff_len) },
        
    { string_make("send_buff_len"), conf_parse_bytes_size,
        OPE_EQUAL, offsetof(conf_server_t, send_buff_len) },
        
    { string_make("max_tqueue_len"), conf_parse_int,
        OPE_EQUAL, offsetof(conf_server_t, max_tqueue_len) },

    { string_make("data_dir"), conf_parse_string,
        OPE_EQUAL, offsetof(conf_server_t, data_dir) },

	{ string_make("heartbeat_interval"), conf_parse_int,
        OPE_EQUAL, offsetof(conf_server_t, heartbeat_interval) },

	{ string_make("block_report_interval"), conf_parse_int,
        OPE_EQUAL, offsetof(conf_server_t, block_report_interval) },

    { string_null, NULL, OPE_EQUAL, 0 }    
};

static conf_object_t dn_conf_objects[] = 
{
    { string_make("Server"), conf_server_init, conf_server_make_default, conf_server_option },

    { string_null, NULL, NULL , NULL}
};

static conf_macro_t conf_macro[] = 
{
    { string_make("ALLOW"), ALLOW },

    { string_make("DENY"), DENY },
    
    { string_make("ON"), CONF_ON},
    
    { string_make("OFF"), CONF_OFF},
    
    { string_make("LOG_FATAL"), DFS_LOG_FATAL },

    { string_make("LOG_ERROR"), DFS_LOG_ERROR },

    { string_make("LOG_ALERT"), DFS_LOG_ALERT },

    { string_make("LOG_WARN"), DFS_LOG_WARN },

    { string_make("LOG_NOTICE"), DFS_LOG_NOTICE },

    { string_make("LOG_INFO"), DFS_LOG_INFO },

    { string_make("LOG_DEBUG"), DFS_LOG_DEBUG },
    
    { string_null, 0 }
};

conf_object_t *get_dn_conf_object(void)
{
    return dn_conf_objects;
}

static void *conf_server_init(pool_t *pool)
{
    conf_server_t *sconf = NULL;
	
    sconf = (conf_server_t *)pool_calloc(pool, sizeof(conf_server_t));
    if (!sconf) 
	{
        return NULL;
    }
	
    if (array_init(&sconf->bind_for_cli, pool, CONF_SERVER_BIND_N, 
		sizeof(server_bind_t)) != DFS_OK) 
    {
        return NULL;
    }
    
    return sconf;
}

static int conf_server_make_default(void *var)
{
    conf_server_t *sconf = (conf_server_t *)var;
    
    set_def_string(&sconf->pid_file,            PID_FILE);
    set_def_int(sconf->recv_buff_len, 		    DEF_RBUFF_LEN);
    set_def_int(sconf->send_buff_len, 		    DEF_SBUFF_LEN);
    set_def_int(sconf->max_tqueue_len, 		    DEF_MMAX_TQUEUE_LEN);
	
    return DFS_OK;
}

static int conf_parse_nn_macro(conf_variable_t *v, uint32_t offset,
    int type, string_t *args, int args_n)
{
    int       i = 0;
    int       vi = 0;
    size_t    len = 0;
    uint32_t *p = 0;
    uchar_t  *start = NULL;
    uchar_t  *end = NULL;
    uchar_t  *pos = NULL;

    if (type != OPE_EQUAL) 
	{
        return DFS_ERROR;
    }

    if (args_n != CONF_TAKE3) 
	{
        return DFS_ERROR;
    }

    vi = args_n - 1;
    if (args[vi].len == 0 || !args[vi].data) 
	{
        return DFS_ERROR;
    }

    p = (uint32_t *)((uchar_t *)v->conf + offset);
    start = pos = args[vi].data;
    end = start + args[vi].len;
    len = end - start;

    if (strchr((char *)start, '|') == NULL) 
	{
        for (i = 0; conf_macro[i].name.len > 0; i++) 
		{
            if (conf_macro[i].name.len == len
                && (string_xxstrncasecmp(start,
                conf_macro[i].name.data, len) == 0)) 
            {
                *p = conf_macro[i].value;
				
                return DFS_OK;
            }
        }
		
        return DFS_ERROR;
    }

    return DFS_ERROR;
}

