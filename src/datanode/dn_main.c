#include <dirent.h>
#include "dn_main.h"
#include "config.h"
#include "dfs_conf.h"
#include "dn_cycle.h"
#include "dn_signal.h"
#include "dn_module.h"
#include "dn_process.h"
#include "dn_conf.h"
#include "dn_time.h"

#define DEFAULT_CONF_FILE PREFIX"/etc/datanode.conf"

#define PATH_LEN  256

int          dfs_argc;
char       **dfs_argv;

string_t     config_file;
static int   g_quit = DFS_FALSE;
static int   show_version;
sys_info_t   dfs_sys_info;
extern pid_t process_pid;

static int parse_cmdline(int argc, char *const *argv);
static int conf_syntax_test(cycle_t *cycle);
static int sys_set_limit(uint32_t file_limit, uint64_t mem_size);
static int sys_limit_init(cycle_t *cycle);

static void dfs_show_help(void)
{
    printf("\t -c, Configure file\n"
        "\t -v, Version\n"
        "\t -q, stop datanode server\n");

    return;
}

static int parse_cmdline( int argc, char *const *argv)
{
    char ch = 0;
    char buf[255] = {0};

    while ((ch = getopt(argc, argv, "c:vqhV")) != -1) 
	{
        switch (ch) 
		{
            case 'c':
                if (!optarg) 
				{
                    dfs_show_help();
					
                    return DFS_ERROR;
                }
				
                if (*optarg == '/') 
				{
                    config_file.data = (uchar_t  *)strdup(optarg);
                    config_file.len = strlen(optarg);
                } 
				else 
				{
                    getcwd(buf,sizeof(buf));
                    buf[strlen(buf)] = '/';
                    strcat(buf,optarg);
                    config_file.data = (uchar_t  *)strdup(buf);
                    config_file.len = strlen(buf);
                }
				
                break;
				
            case 'V':
            case 'v':
                printf("datanode version: \"PACKAGE_STRING\"\n");
#ifdef CONFIGURE_OPTIONS
                printf("configure option:\"CONFIGURE_OPTIONS\"\n");
#endif
                show_version = 1;

                exit(0);
				
                break;
				
            case 'q':
                g_quit = DFS_TRUE;
                break;
				
            case 'h':
				
            default:
                dfs_show_help();
				
                return DFS_ERROR;
        }
    }

    return DFS_OK;
}

int main(int argc, char **argv)
{
    int            ret = DFS_OK;
    cycle_t       *cycle = NULL;
    conf_server_t *sconf = NULL;
    
    cycle = cycle_create();

    time_init();

    if (parse_cmdline(argc, argv) != DFS_OK) 
	{
        return DFS_ERROR;
    }

    if (!show_version && sys_get_info(&dfs_sys_info) != DFS_OK) 
	{
        return DFS_ERROR;
    }
 
    if (config_file.data == NULL) 
	{
        config_file.data = (uchar_t *)strndup(DEFAULT_CONF_FILE,
            strlen(DEFAULT_CONF_FILE));
        config_file.len = strlen(DEFAULT_CONF_FILE);
    }
    
    if (g_quit) 
	{
        if ((ret = cycle_init(cycle))!= DFS_OK) 
		{
            fprintf(stderr, "cycle_init fail\n");
			
            goto out;
        }
		
        process_pid = process_get_pid(cycle);
        if (process_pid < 0)
		{
            fprintf(stderr, " get server pid fail\n");
            ret = DFS_ERROR;
			
            goto out;
        }

        kill(process_pid, SIGNAL_QUIT);
        printf("service is stoped\n");
		
        ret = DFS_OK;
		
        goto out;
    }

    umask(0022);
    
    if ((ret = cycle_init(cycle)) != DFS_OK) 
	{
        fprintf(stderr, "cycle_init fail\n");
		
        goto out;
    }

    if ((ret = process_check_running(cycle)) == DFS_TRUE) 
	{
        fprintf(stderr, "datanode is already running\n");
		
    	goto out;
    }

    if ((ret = sys_limit_init(cycle)) != DFS_OK)
	{
        fprintf(stderr, "sys_limit_init error\n");
		
    	goto out;
    }
    
	dfs_module_setup();

	if ((ret = dfs_module_master_init(cycle)) != DFS_OK) 
	{
		fprintf(stderr, "master init fail\n");
		
        goto out;
	}
    
    sconf = (conf_server_t *)cycle->sconf;

    if (process_change_workdir(&sconf->coredump_dir) != DFS_OK) 
	{
        dfs_log_error(cycle->error_log, DFS_LOG_FATAL, 0,
                "process_change_workdir failed!\n");
		
        goto failed;
    }

    if (dn_signal_setup() != DFS_OK) 
	{
        dfs_log_error(cycle->error_log, DFS_LOG_FATAL, 0,
                "setup signal failed!\n");
		
        goto failed;
    }
    
    if (sconf->daemon == DFS_TRUE && dn_daemon() == DFS_ERROR) 
	{
        dfs_log_error(cycle->error_log, DFS_LOG_FATAL, 0,
                "dfs_daemon failed");
		
        goto failed;
    }

    process_pid = getpid();
	
    if (process_write_pid_file(process_pid) == DFS_ERROR) 
	{
        dfs_log_error(cycle->error_log, DFS_LOG_WARN, 0, 
                "write pid file error");
		
        goto failed;
    }

    dfs_argc = argc;
    dfs_argv = argv;

    process_master_cycle(cycle, dfs_argc, dfs_argv);

    process_del_pid_file();

failed:
    dfs_module_master_release(cycle);

out:
    if (config_file.data) 
	{
        free(config_file.data);
        config_file.len = 0;
    }
	
    if (cycle) 
	{
        cycle_free(cycle);
    }

    return ret;
}

int dn_daemon()
{
    int fd = DFS_INVALID_FILE;
    int pid = DFS_ERROR;
	
    pid = fork();

    if (pid > 0) 
	{
        exit(0);
    } 
	else if (pid < 0) 
	{
        printf("dfs_daemon: fork failed\n");
		
        return DFS_ERROR;
    }
	
    if (setsid() == DFS_ERROR) 
	{
        printf("dfs_daemon: setsid failed\n");
		
        return DFS_ERROR;
    }

    umask(0022);

    fd = open("/dev/null", O_RDWR);
    if (fd == DFS_INVALID_FILE) 
	{
        return DFS_ERROR;
    }

    if (dup2(fd, STDIN_FILENO) == DFS_ERROR) 
	{
        printf("dfs_daemon: dup2(STDIN) failed\n");
		
        return DFS_INVALID_FILE;
    }

    if (dup2(fd, STDOUT_FILENO) == DFS_ERROR) 
	{
        printf("dfs_daemon: dup2(STDOUT) failed\n");
		
        return DFS_ERROR;
    }

    if (dup2(fd, STDERR_FILENO) == DFS_ERROR) 
	{
        printf("dfs_daemon: dup2(STDERR) failed\n");
		
        return DFS_ERROR;
    }

    if (fd > STDERR_FILENO) 
	{
        if (close(fd) == DFS_ERROR) 
		{
            printf("dfs_daemon: close() failed\n");
			
            return DFS_ERROR;
        }
    }

    return DFS_OK;
}

static int sys_set_limit(uint32_t file_limit, uint64_t mem_size)
{
    int            ret = DFS_ERROR;
    int            need_set = DFS_FALSE;
    struct rlimit  rl;
    log_t         *log;

    log = dfs_cycle->error_log;

    ret = getrlimit(RLIMIT_NOFILE, &rl);
    if (ret == DFS_ERROR) 
	{
        dfs_log_error(log, DFS_LOG_ERROR,
            errno, "sys_set_limit get RLIMIT_NOFILE error");
		
        return ret;
    }
	
    if (rl.rlim_max < file_limit) 
	{
        rl.rlim_max = file_limit;
        need_set = DFS_TRUE;
    }
	
    if (rl.rlim_cur < file_limit) 
	{
        rl.rlim_cur = file_limit;
        need_set = DFS_TRUE;
    }
	
    if (need_set) 
	{
        ret = setrlimit(RLIMIT_NOFILE, &rl);
        if (ret == DFS_ERROR) 
		{
            dfs_log_error(log, DFS_LOG_ERROR,
                errno, "sys_set_limit set RLIMIT_NOFILE error");
			
            return ret;
        }
    }

    // set mm overcommit policy to use large block memory
    if (mem_size > ((size_t)1 << 32)) 
	{
        ret = system("sysctl -w vm.overcommit_memory=1 > /dev/zero");
        if (ret == DFS_ERROR) 
		{
            dfs_log_error(log, DFS_LOG_ERROR,
                errno, "sys_set_limit set vm.overcommit error");
			
            return DFS_ERROR;
        }
    }

    // enable core dump
    if (prctl(PR_SET_DUMPABLE, 1, 0, 0, 0) != 0) 
	{
        dfs_log_error(log, DFS_LOG_ERROR,
            errno, "sys_set_limit set PR_SET_DUMPABLE error");
		
        return DFS_ERROR;
    }
	
    if (getrlimit(RLIMIT_CORE, &rl) == 0) 
	{
        rl.rlim_cur = rl.rlim_max;
		
        ret = setrlimit(RLIMIT_CORE, &rl);
        if (ret == DFS_ERROR) 
		{
            dfs_log_error(log, DFS_LOG_ERROR,
                errno, "sys_set_limit set RLIMIT_CORE error");
			
            return DFS_ERROR;
        }
    }

    return DFS_OK;
}

static int sys_limit_init(cycle_t *cycle)
{
    conf_server_t *sconf = (conf_server_t *)cycle->sconf;
    
    return sys_set_limit(sconf->connection_n, 0);
}

