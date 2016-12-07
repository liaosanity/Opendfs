#include <dirent.h>
#include "nn_main.h"
#include "config.h"
#include "dfs_conf.h"
#include "nn_cycle.h"
#include "nn_signal.h"
#include "nn_module.h"
#include "nn_process.h"
#include "nn_conf.h"
#include "nn_time.h"

#define DEFAULT_CONF_FILE PREFIX"/etc/namenode.conf"

#define PATH_LEN  256

int          dfs_argc;
char       **dfs_argv;

string_t     config_file;
static int   g_format = DFS_FALSE;
static int   test_conf = DFS_FALSE;
static int   g_reconf = DFS_FALSE;
static int   g_quit = DFS_FALSE;
static int   show_version;
sys_info_t   dfs_sys_info;
extern pid_t process_pid;

static int parse_cmdline(int argc, char *const *argv);
static int conf_syntax_test(cycle_t *cycle);
static int sys_set_limit(uint32_t file_limit, uint64_t mem_size);
static int sys_limit_init(cycle_t *cycle);
static int format(cycle_t *cycle);
static int clear_current_dir(cycle_t *cycle);
static int save_ns_version(cycle_t *cycle);
static int get_ns_version(cycle_t *cycle);

static void dfs_show_help(void)
{
    printf("\t -c, Configure file\n"
		"\t -f, format the DFS filesystem\n"
        "\t -v, Version\n"
        "\t -t, Test configure\n"
        "\t -r, Reload configure file\n"
        "\t -q, stop namenode server\n");

    return;
}

static int parse_cmdline( int argc, char *const *argv)
{
    char ch = 0;
    char buf[255] = {0};

    while ((ch = getopt(argc, argv, "c:fvtrqhV")) != -1) 
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

			case 'f':
				g_format = DFS_TRUE;
				break;
				
            case 'V':
            case 'v':
                printf("namenode version: \"PACKAGE_STRING\"\n");
#ifdef CONFIGURE_OPTIONS
                printf("configure option:\"CONFIGURE_OPTIONS\"\n");
#endif
                show_version = 1;

                exit(0);
				
                break;
				
            case 't':
                test_conf = DFS_TRUE;
                break;
				
            case 'r':
                g_reconf= DFS_TRUE;
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
    
    if (test_conf == DFS_TRUE) 
	{
        ret = conf_syntax_test(cycle);
		
        goto out;
    }
    
    if (g_reconf || g_quit) 
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

        if (g_reconf) 
		{
            kill(process_pid, SIGNAL_RECONF);
            printf("config is reloaded\n");
        }
		else 
		{
            kill(process_pid, SIGNAL_QUIT);
            printf("service is stoped\n");
        }
		
        ret = DFS_OK;
		
        goto out;
    }

    umask(0022);
    
    if ((ret = cycle_init(cycle)) != DFS_OK) 
	{
        fprintf(stderr, "cycle_init fail\n");
		
        goto out;
    }

	if (g_format) 
	{
	    format(cycle);
		
        goto out;
	}

    if ((ret = process_check_running(cycle)) == DFS_TRUE) 
	{
        fprintf(stderr, "namenode is already running\n");
		
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
			"process_change_workdir failed!");
		
        goto failed;
    }

    if (signal_setup() != DFS_OK) 
	{
        dfs_log_error(cycle->error_log, DFS_LOG_FATAL, 0, 
			"setup signal failed!");
		
        goto failed;
    }
    
    if (sconf->daemon == DFS_TRUE && nn_daemon() == DFS_ERROR) 
	{
        dfs_log_error(cycle->error_log, DFS_LOG_FATAL, 0, 
			"dfs_daemon failed");
		
        goto failed;
    }

    process_pid = getpid();
	
    if (process_write_pid_file(process_pid) == DFS_ERROR) 
	{
        dfs_log_error(cycle->error_log, DFS_LOG_FATAL, 0, 
			"write pid file error");
		
        goto failed;
    }

	if (get_ns_version(cycle) != DFS_OK) 
	{
	    dfs_log_error(cycle->error_log, DFS_LOG_FATAL, 0, 
			"the DFS filesystem is unformatted");
		
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

int nn_daemon()
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

static int conf_syntax_test(cycle_t *cycle)
{
    if (config_file.data == NULL)
	{
        return DFS_ERROR;
    }

    if (cycle_init(cycle) == DFS_ERROR) 
	{
        return DFS_ERROR;
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

static int format(cycle_t *cycle)
{
    conf_server_t *sconf = (conf_server_t *)cycle->sconf;

	fprintf(stdout, "Re-format filesystem in %s ? (Y or N)\n", 
		sconf->fsimage_dir.data);

	char input;
	scanf("%c", &input);

	if (input == 'Y') 
	{
		if (clear_current_dir(cycle) != DFS_OK) 
		{
            return DFS_ERROR;
		}

		if (save_ns_version(cycle) != DFS_OK) 
		{
            return DFS_ERROR;
		}

		fprintf(stdout, "Storage directory %s has been successfully formatted.\n", 
		    sconf->fsimage_dir.data);
	}
	else 
	{
        fprintf(stdout, "Format aborted in %s\n", sconf->fsimage_dir.data);
	}
	
    return DFS_OK;
}

static int clear_current_dir(cycle_t *cycle)
{
    conf_server_t *sconf = (conf_server_t *)cycle->sconf;
	
    char dir[PATH_LEN] = {0};
	string_xxsprintf((uchar_t *)dir, "%s/current", sconf->fsimage_dir.data);

	if (access(dir, F_OK) != DFS_OK) 
	{
	    if (mkdir(dir, S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH) != DFS_OK) 
	    {
	        dfs_log_error(dfs_cycle->error_log, DFS_LOG_ERROR, errno, 
				"mkdir %s err", dir);
		
	        return DFS_ERROR;
	    }

		return DFS_OK;
	}
	
	DIR *dp = NULL;
    struct dirent *entry = NULL;
	
    if ((dp = opendir(dir)) == NULL) 
	{
        printf("open %s err: %s\n", dir, strerror(errno));
		
        return DFS_ERROR;
    }

	while ((entry = readdir(dp)) != NULL) 
	{
		if (entry->d_type == 8)
		{
		    // file
            char sBuf[PATH_LEN] = {0};
 		    sprintf(sBuf, "%s/%s", dir, entry->d_name);
 		    printf("unlink %s\n", sBuf);
 
 		    unlink(sBuf);
		}
		else if (0 != strcmp(entry->d_name, ".") 
			&& 0 != strcmp(entry->d_name, ".."))
		{
            // sub-dir
		}
    }
	
	closedir(dp);
		
    return DFS_OK;
}

static int save_ns_version(cycle_t *cycle)
{
    conf_server_t *sconf = (conf_server_t *)cycle->sconf;

	char v_name[PATH_LEN] = {0};
	string_xxsprintf((uchar_t *)v_name, "%s/current/VERSION", 
		sconf->fsimage_dir.data);
	
	int fd = open(v_name, O_RDWR | O_CREAT | O_TRUNC, 0664);
	if (fd < 0) 
	{
		printf("open %s err: %s\n", v_name, strerror(errno));
		
        return DFS_ERROR;
	}

	int64_t namespaceID = time_curtime();

	char ns_version[256] = {0};
	sprintf(ns_version, "namespaceID=%ld\n", namespaceID);

	if (write(fd, ns_version, strlen(ns_version)) < 0) 
    {
		printf("write %s err: %s\n", v_name, strerror(errno));

		return DFS_ERROR;
	}

	close(fd);
	
    return DFS_OK;
}

static int get_ns_version(cycle_t *cycle)
{
    conf_server_t *sconf = (conf_server_t *)cycle->sconf;

	char v_name[PATH_LEN] = {0};
	string_xxsprintf((uchar_t *)v_name, "%s/current/VERSION", 
		sconf->fsimage_dir.data);
	
	int fd = open(v_name, O_RDONLY);
	if (fd < 0) 
	{
		dfs_log_error(dfs_cycle->error_log, DFS_LOG_ALERT, errno, 
			"open %s err", v_name);
		
        return DFS_ERROR;
	}

    char ns_version[256] = {0};
	if (read(fd, ns_version, sizeof(ns_version)) < 0) 
    {
		dfs_log_error(dfs_cycle->error_log, DFS_LOG_ERROR, errno, 
			"read %s err", v_name);

		close(fd);

		return DFS_ERROR;
	}

	sscanf(ns_version, "%*[^=]=%ld", &cycle->namespace_id);

	close(fd);
	
    return DFS_OK;
}

