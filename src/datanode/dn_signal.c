#include "dn_signal.h"
#include "dfs_conf.h"
#include "dfs_memory.h"
#include "dfs_error_log.h"
#include "dn_process.h"

extern uint32_t  process_doing;
extern uint32_t  process_type;  
extern int       process_slot;
extern process_t processes[];
extern int       process_slot;
extern process_t processes[];

static void signal_handler(int sig, siginfo_t *info, void *context);

int dn_signal_setup(void)
{
    int ret = DFS_ERROR;
    
    //if we don't use aio thread method(use aio signal method),
    //we set aio_callback handler for SIGRTMIN + 1 signal
#if !USE_AIO_THREAD
    struct sigaction sig_act;

    sigemptyset(&sig_act.sa_mask);
    sig_act.sa_flags = SA_SIGINFO;
    //sig_act.sa_sigaction = file_aio_callback;
    ret = sigaction(SIGRTMIN + 1, &sig_act, NULL);
    if (ret != DFS_OK) 
	{
        return DFS_ERROR;
    }
#endif
    //block IO and SIGRTMIN + 1 signal
    //sigemptyset(&set);
    //sigaddset(&set, SIGIO);
    //sigaddset(&set, SIGRTMIN + 1);
    //sigprocmask(SIG_BLOCK, &set, 0);
    
    //set other sig handler
    sig_act.sa_sigaction = signal_handler;
    ret = sigaction(SIGALRM, &sig_act, NULL);
    if (ret != DFS_OK) 
	{
        return DFS_ERROR;
    }
    
    ret = sigaction(SIGINT, &sig_act, NULL);
    if (ret != DFS_OK) 
	{
        return DFS_ERROR;
    }
    
    ret = sigaction(SIGIO, &sig_act, NULL);
    if (ret != DFS_OK) 
	{
        return DFS_ERROR;
    }
    
    ret = sigaction(SIGCHLD, &sig_act, NULL);
    if (ret != DFS_OK) 
	{
        return DFS_ERROR;
    }
    
    ret = sigaction(SIGPIPE, &sig_act, NULL);
    if (ret != DFS_OK) 
	{
        return DFS_ERROR;
    }
    
    ret = sigaction(SIGSEGV, &sig_act, NULL);
    if (ret != DFS_OK) 
	{
        return DFS_ERROR;
    }
    
    ret = sigaction(SIGNAL_RECONF, &sig_act, NULL);
    if (ret != DFS_OK) 
	{
        return DFS_ERROR;
    }
    
    ret = sigaction(SIGNAL_QUIT, &sig_act, NULL);
    if (ret != DFS_OK) 
	{
        return DFS_ERROR;
    }
    
    ret = sigaction(SIGNAL_TERMINATE, &sig_act, NULL);
    if (ret != DFS_OK) 
	{
        return DFS_ERROR;
    }

    ret = sigaction(SIGNAL_TEST_STORE, &sig_act, NULL);
    if (ret != DFS_OK) 
	{
        return DFS_ERROR;
    }
	
    ret = sigaction(SIGUSR1, &sig_act, NULL);
    if (ret != DFS_OK) 
	{
        return DFS_ERROR;
    }
 
    return ret;
}

static void signal_handler(int sig, siginfo_t *info, void *context)
{
    void      *fun_array[8192];
    char     **fun_name = NULL;
    size_t     sz = 0;
    uint32_t   j = 0;

    switch (sig) 
	{
    //signals that will be processed
    case SIGALRM:
        dfs_log_debug(dfs_cycle->error_log, DFS_LOG_DEBUG, 
            0, "signal_handler: recv SIGALARM");
		
        process_doing |= PROCESS_DOING_ALARM;
        return;
		
    case SIGNAL_QUIT:
        dfs_log_debug(dfs_cycle->error_log, DFS_LOG_DEBUG, 
            0, "signal_handler: recv SIGNAL_QUIT");
		
        process_doing |= PROCESS_DOING_QUIT;
        return;
		
    case SIGINT:
    case SIGNAL_TERMINATE:
        dfs_log_debug(dfs_cycle->error_log, DFS_LOG_DEBUG, 
            0, "signal_handler: recv SIGNAL_TERMINATE or SIGINT");
		
        process_doing |= PROCESS_DOING_TERMINATE;
        return;
		
    case SIGNAL_RECONF:
        dfs_log_debug(dfs_cycle->error_log, DFS_LOG_DEBUG, 
            0, "signal_handler: recv SIGNAL_RECONF");
		
        process_doing |= PROCESS_DOING_RECONF;
        return;
		
    case SIGCHLD:
        dfs_log_debug(dfs_cycle->error_log, DFS_LOG_DEBUG, 
            0, "signal_handler: recv SIGCHLD");
		
        process_doing |= PROCESS_DOING_REAP;
        return;
		
    case SIGNAL_TEST_STORE:
        process_doing |= PROCESS_DOING_TEST_STORE;
        return;
		
    case SIGUSR1:
        process_doing |= PROCESS_DOING_BACKUP;
        return;
		
    //ignored signals
    case SIGIO:
        dfs_log_debug(dfs_cycle->error_log, DFS_LOG_DEBUG, 
            0, "signal_handler: recv SIGIO, ignored");
		
        return;
		
    case SIGPIPE:
        /*dfs_log_debug(dfs_cycle->error_log, DFS_LOG_DEBUG, 
            0, "signal_handler: exit with sig SIGPIPE");*/
            
        return;
		
    //error signals
    case SIGBUS:
        dfs_log_error(dfs_cycle->error_log, DFS_LOG_INFO, 
            0, "signal_handler: exit with sig SIGBUS");
		
        break;
		
    case SIGSEGV:
        dfs_log_error(dfs_cycle->error_log, DFS_LOG_INFO, 
            0, "signal_handler: pid %d slot %d exit with sig SIGSEGV",
            getpid(), process_slot);
		
        break;
    }
   
    sz = backtrace(fun_array, 8192);
    fun_name = backtrace_symbols(fun_array, sz);
	
    for (j = 0; j < sz; j++)
	{
        dfs_log_error(dfs_cycle->error_log, DFS_LOG_FATAL, 0, 
			"Fun: %s", fun_name[j]);
    }

    // must free the arr by the caller of backtrace_symbols
    memory_free(fun_name, sizeof(fun_name));
   
    if (process_type == PROCESS_MASTER) 
	{
        process_del_pid_file();
    }

    abort(); 
}

