#include <sys/ioctl.h>

#include "dn_process.h"
#include "dfs_error_log.h"
#include "dfs_conn.h"
#include "dfs_channel.h"
#include "dfs_memory.h"
#include "dfs_lock.h"
#include "dfs_conn_listen.h"
#include "dn_signal.h"
#include "dn_module.h"
#include "dn_thread.h"
#include "dn_time.h"
#include "dn_conf.h"
#include "dn_worker_process.h"
#include "dn_conn_event.h"

#define MASTER_TITLE "datanode: master process"

#define    MAX_TIMER 5

int        process_slot;             // process' slot
int        process_last;
pid_t      process_pid;
uint32_t   process_doing = 0;        // the action that process will do
uint32_t   process_type;
uint32_t   stop;

process_t  processes[PROCESSES_MAX]; // gloabal processes' info

extern char   **environ;
extern char   **dfs_argv;

static int      process_old_alived = DFS_FALSE;
dfs_thread_t   *main_thread = NULL;

static int process_reap_workers(cycle_t *cycle);

int process_check_running(cycle_t *cycle)
{
    conf_server_t *sconf = NULL;
    pid_t          pid = -1;
    char          *pid_file = NULL;
    struct stat    st;

    sconf = (conf_server_t *)dfs_cycle->sconf;

    pid_file = (char *)sconf->pid_file.data;

    if (stat(pid_file, &st) < 0) 
	{
        return DFS_FALSE;
    }

    pid = process_get_pid(cycle);
    if (pid == (pid_t)DFS_ERROR) 
	{
   	    return DFS_FALSE;
    }

    if (kill(pid, 0) < 0) 
	{
   	    return DFS_FALSE;
    }

    return DFS_TRUE;
}

static pid_t process_spawn(cycle_t *cycle, spawn_proc_pt proc, 
	                              void *data, char *name, int slot)
{
    pid_t     pid = -1;
    uint64_t  on = 0;
    log_t    *log = NULL;
    
    log = cycle->error_log;

    if (slot == PROCESS_SLOT_AUTO) 
	{
        for (slot = 0; slot < process_last; slot++) 
		{
            if (processes[slot].pid == DFS_INVALID_PID) 
			{
                break;
            }
        }
		
        if (slot == PROCESSES_MAX) 
		{
            dfs_log_error(log, DFS_LOG_WARN, 0,
                "no more than %d processes can be spawned", PROCESSES_MAX);
			
            return DFS_INVALID_PID;
        }
    }

    errno = 0;

    if (socketpair(AF_UNIX, SOCK_STREAM, 0, processes[slot].channel) == DFS_ERROR)
    {
        return DFS_INVALID_PID;
    }

    if (conn_nonblocking(processes[slot].channel[0]) == DFS_ERROR) 
	{
        channel_close(processes[slot].channel, cycle->error_log);

        return DFS_INVALID_PID;
    }

    if (conn_nonblocking(processes[slot].channel[1]) == DFS_ERROR) 
	{
        channel_close(processes[slot].channel, cycle->error_log);

        return DFS_INVALID_PID;
    }

    on = 1;

    if (ioctl(processes[slot].channel[0], FIOASYNC, &on) == DFS_ERROR) 
	{
        channel_close(processes[slot].channel, cycle->error_log);

        return DFS_INVALID_PID;
    }

    if (fcntl(processes[slot].channel[0], F_SETOWN, process_pid) == DFS_ERROR) 
	{
        return DFS_INVALID_PID;
    }

    if (fcntl(processes[slot].channel[0], F_SETFD, FD_CLOEXEC) == DFS_ERROR) 
	{
        channel_close(processes[slot].channel, cycle->error_log);

        return DFS_INVALID_PID;
    }

    if (fcntl(processes[slot].channel[1], F_SETFD, FD_CLOEXEC) == DFS_ERROR) 
	{
        channel_close(processes[slot].channel, cycle->error_log);

        return DFS_INVALID_PID;
    }

    process_slot = slot;

    pid = fork();

    switch (pid) 
	{
        case DFS_INVALID_PID:

            channel_close(processes[slot].channel, cycle->error_log);

            return DFS_INVALID_PID;
			
        case 0:

            process_pid = getpid();
            
            proc(cycle, data);

            return DFS_INVALID_PID;
			
        default:

            break;
    }

    processes[slot].pid = pid;
    processes[slot].proc = proc;
    processes[slot].data = data;
    processes[slot].name = name;
    processes[slot].ps = PROCESS_STATUS_RUNNING;
    processes[slot].ow  = DFS_FALSE;
    processes[slot].restart_gap = dfs_time->tv_sec;

    if (slot == process_last) 
	{
        process_last++;
    }

    return pid;
}


void process_broadcast(int slot, int cmd)
{
    int       s = -1;
    channel_t ch;

    ch.command = cmd;
    ch.pid = processes[slot].pid;
    ch.slot = slot;
    ch.fd = processes[slot].channel[0];

    for (s = 0; s < process_last; s++) 
	{
        if ((s == process_slot)
            || (processes[s].ps & PROCESS_STATUS_EXITED)
            || (processes[s].ps & PROCESS_STATUS_EXITING)
            || (processes[s].pid == DFS_INVALID_PID)
            || (processes[s].channel[0] == DFS_INVALID_FILE)
            || (processes[s].ow == DFS_TRUE))
        {
            continue;
        }

		// broadcast the new process's channel to all other processes
        channel_write(processes[s].channel[0], &ch, sizeof(channel_t), 
            dfs_cycle->error_log);
    }
}

void process_set_title(string_t *title)
{
    // copy the title to argv[0], and set argv[1] to NULL
    string_strncpy(dfs_argv[0], title->data, title->len);

    dfs_argv[0][title->len] = 0;
    dfs_argv[1] = NULL;
}

void process_get_status()
{
    int   i = 0;
    int   status = 0;
    pid_t pid = -1;

    for ( ;; ) 
	{
        pid = waitpid(-1, &status, WNOHANG);

        if (pid == 0) 
		{
            return;
        }

        if (pid == DFS_INVALID_PID) 
		{

            if (errno == DFS_EINTR) 
			{
                continue;
            }

            return;
        }

        for (i = 0; i < process_last; i++) 
		{
            if (processes[i].pid == pid) 
			{
                processes[i].status = status;
                processes[i].ps = PROCESS_STATUS_EXITED;

                break;
            }
        }

        processes[i].status = status;
        processes[i].ps = PROCESS_STATUS_EXITED;
    }
}

int process_change_workdir(string_t *dir)
{
    if (chdir((char*)dir->data) < 0) 
	{
        dfs_log_error(dfs_cycle->error_log, DFS_LOG_FATAL, errno,
            "process_change_workdir failed!\n");

        return DFS_ERROR;
    }

    return DFS_OK;
}

void process_set_old_workers()
{
    int i = 0;

    for (i = 0; i < process_last; i++) 
	{
        if (processes[i].pid != DFS_INVALID_PID
            && (processes[i].ps & PROCESS_STATUS_RUNNING)) 
        {
            processes[i].ow = DFS_TRUE;
        }
    }
}

void process_signal_workers(int signo)
{
    int       i = 0;
    channel_t ch;

    switch (signo) 
	{
        case SIGNAL_QUIT:
            ch.command = CHANNEL_CMD_QUIT;
            break;

        case SIGNAL_TERMINATE:
            ch.command = CHANNEL_CMD_TERMINATE;
            break;
			
        default:
            ch.command = CHANNEL_CMD_NONE;
            break;
    }

    ch.fd = DFS_INVALID_FILE;

    for (i = 0; i < process_last; i++) 
	{
        if (processes[i].pid == DFS_INVALID_PID
            || processes[i].ow == DFS_FALSE) 
        {
            continue;
        }

        if (ch.command != CHANNEL_CMD_NONE) 
		{
            if (channel_write(processes[i].channel[0], &ch,
                sizeof(channel_t), dfs_cycle->error_log) == DFS_OK)
            {
                processes[i].ps |= PROCESS_STATUS_EXITING;
                processes[i].ow = DFS_FALSE;
				
                continue;
            }
        }

        dfs_log_debug(dfs_cycle->error_log, DFS_LOG_DEBUG, 0,
            "kill (%P, %d)", processes[i].pid, signo);
		
        if (kill(processes[i].pid, signo) == DFS_ERROR) 
		{
            dfs_log_error(dfs_cycle->error_log, DFS_LOG_ALERT, errno,
                "kill(%P, %d) failed", processes[i].pid, signo);
        }
		
        processes[i].pid = DFS_INVALID_PID;
        channel_close(processes[i].channel, dfs_cycle->error_log);
        processes[i].channel[0] = DFS_INVALID_FILE;
        processes[i].channel[1] = DFS_INVALID_FILE;
        processes[i].ps = 0;
        processes[i].ow = DFS_FALSE;
    }
}

void process_notify_workers_backup()
{
    int       idx = 0;
    channel_t ch;
	
    ch.fd = DFS_INVALID_FILE;
    ch.command = CHANNEL_CMD_BACKUP;

    for (idx = 0; idx < process_last; idx++) 
	{
        if (processes[idx].pid == DFS_INVALID_PID) 
		{
            continue;
        }

        if (channel_write(processes[idx].channel[0], &ch,
                    sizeof(channel_t), dfs_cycle->error_log) != DFS_OK) 
        {
            dfs_log_error(dfs_cycle->error_log, DFS_LOG_WARN,
                    0, "send backup command error!");
        }
    }

    return;
}

int process_start_workers(cycle_t *cycle)
{
    dfs_log_debug(cycle->error_log, DFS_LOG_DEBUG, 0, "process_start_workers");

    if (process_spawn(cycle, worker_processer, NULL,
        (char *)"worker process", PROCESS_SLOT_AUTO) == DFS_INVALID_PID) 
    {
        return DFS_ERROR;
    }

    process_broadcast(process_slot, CHANNEL_CMD_OPEN);

    return DFS_OK;
}

void process_master_cycle(cycle_t *cycle, int argc, char **argv)
{
    int       i = 0;
    int       live = 1;
    size_t    size = 0;
    uchar_t  *p_title = NULL;
    sigset_t  set;
    string_t  title;

    sigemptyset(&set);
    sigaddset(&set, SIGCHLD);
    sigaddset(&set, SIGALRM);
    sigaddset(&set, SIGIO);
    sigaddset(&set, SIGINT);
    sigaddset(&set, SIGNAL_RECONF);
    sigaddset(&set, SIGNAL_TERMINATE);
    sigaddset(&set, SIGNAL_QUIT);
    sigaddset(&set, SIGNAL_TEST_STORE);

    if (sigprocmask(SIG_BLOCK, &set, NULL) == DFS_ERROR) 
	{
        dfs_log_error(cycle->error_log, DFS_LOG_ALERT, errno,
            "sigprocmask() failed");
    }

    process_type = PROCESS_MASTER;
	
	thread_env_init();
	
	main_thread = thread_new(NULL);
    if (!main_thread) 
	{
        return;
    }
	
    main_thread->type = THREAD_MASTER;
    thread_bind_key(main_thread);
	
    size = sizeof(MASTER_TITLE) - 1;

    for (i = 0; i < argc; i++) 
	{
        size += string_strlen(argv[i]) + 1;
    }

    title.data = (uchar_t *)pool_alloc(cycle->pool, size);
    if (!title.data) 
	{
        return;
    }

    p_title = memory_cpymem(title.data, MASTER_TITLE,
        sizeof(MASTER_TITLE) - 1);

    for (i = 0; i < argc; i++) 
	{
        *p_title++ = ' ';
        p_title = memory_cpymem(p_title, argv[i], string_strlen(argv[i]));
    }

    title.len = size;

    process_set_title(&title);

    memset(processes, 0x00, sizeof(processes));

    for (i = 0; i < PROCESSES_MAX; i++) 
	{
        processes[i].pid = DFS_INVALID_PID;
    }

	accept_lock_init();
    
    if (conn_listening_init(cycle) != DFS_OK) 
	{
        return;
    }

    if (process_start_workers(cycle) != DFS_OK) 
	{
        return;
    }

    sigemptyset(&set);

    for ( ;; ) 
	{
        dfs_log_debug(cycle->error_log, DFS_LOG_DEBUG, 0, "sigsuspend");

        sigsuspend(&set);

        time_update();

        while (process_doing) 
		{
            if (!stop && process_doing & PROCESS_DOING_REAP) 
			{
                process_doing &= ~PROCESS_DOING_REAP;

                process_get_status();

                dfs_log_debug(cycle->error_log, DFS_LOG_DEBUG, 0, 
					"reap children");

                live = process_reap_workers(cycle);

                process_old_alived = DFS_FALSE;
				
                continue;
            }

            if (!live && ((process_doing & PROCESS_DOING_QUIT) 
                || (process_doing & PROCESS_DOING_TERMINATE))) 
            {
                return;
            }

            if ((process_doing & PROCESS_DOING_QUIT)) 
			{

                // process_doing &= ~PROCESS_DOING_QUIT;

                if (!stop) 
				{
                    stop = 1;
					
                    process_set_old_workers();
                    process_signal_workers(SIGNAL_QUIT);
                    process_get_status();

                    live = 0;
                }

                break;
            }

            if ((process_doing & PROCESS_DOING_TERMINATE)) 
			{
                //process_doing &= ~PROCESS_DOING_TERMINATE;

                if (!stop) 
				{
                    stop = 1;

                    process_set_old_workers();
                    process_signal_workers(SIGNAL_KILL);
                    process_get_status();

                    live = 0;
                }

                break;
            }
            
            if (process_doing & PROCESS_DOING_RECONF) 
			{
                process_set_old_workers();
                process_signal_workers(SIGNAL_QUIT);
                process_doing &= ~PROCESS_DOING_RECONF;
                process_get_status();

                live = process_reap_workers(cycle);
				
                process_old_alived = DFS_FALSE;
            }

            if (process_doing & PROCESS_DOING_BACKUP && live == DFS_TRUE) 
			{
                process_notify_workers_backup();
                process_doing &= ~PROCESS_DOING_BACKUP;
            }
        }
    }
	
    exit(-1);
}

static int process_reap_workers(cycle_t *cycle)
{
    int i = 0;
    int live = DFS_FALSE;
	
    for (i = 0; i < process_last; i++) 
	{
        if (processes[i].pid == DFS_INVALID_PID)
		{
            continue;
        }

        if (!(processes[i].ps & PROCESS_STATUS_EXITED)) 
		{
            live = DFS_TRUE;
			
            if (processes[i].ow == DFS_TRUE) 
			{
                process_old_alived = DFS_TRUE;
            }
			
            continue;
        }
		
        if (processes[i].ow == DFS_TRUE) 
		{
            processes[i].pid = DFS_INVALID_PID;
            processes[i].ow = 0;
            channel_close(processes[i].channel, cycle->error_log);
            processes[i].channel[0] = DFS_INVALID_FILE;
            processes[i].channel[1] = DFS_INVALID_FILE;           
            continue;
        }
		
        // detach this process
        processes[i].pid = DFS_INVALID_PID;
        processes[i].ps = PROCESS_STATUS_EXITED;
        channel_close(processes[i].channel, cycle->error_log);
        processes[i].channel[0] = DFS_INVALID_FILE;
        processes[i].channel[1] = DFS_INVALID_FILE;
        // respawn the process if need

        if (dfs_time->tv_sec - processes[i].restart_gap <= MAX_RESTART_NUM) 
		{
            dfs_log_error(cycle->error_log, DFS_LOG_ALERT, 0,
                "over max restart num %s", processes[i].name);
			
            continue;
        }
		
        if (process_spawn(cycle, processes[i].proc,
            processes[i].data, processes[i].name, i)
            == DFS_INVALID_PID) 
        {
            dfs_log_error(cycle->error_log, DFS_LOG_ALERT, 0,
                "can not respawn %s", processes[i].name);
			
            continue;
        }

        live = DFS_TRUE;
    }

    return live;
}

int process_write_pid_file(pid_t pid)
{
    int            fd = -1;
    ssize_t        n = 0;
    uchar_t        buf[10] = {0};
    uchar_t       *last = 0;
    uchar_t       *pid_file = NULL;
    conf_server_t *sconf = NULL;

    sconf = (conf_server_t *)dfs_cycle->sconf; 
    if (!sconf) 
	{
        return DFS_ERROR;
    }
	
    pid_file = sconf->pid_file.data;
    last = string_xxsnprintf(buf, 10, "%d", pid);
	
    fd = dfs_sys_open(pid_file, O_RDWR | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) 
	{
        dfs_log_error(dfs_cycle->error_log, DFS_LOG_WARN, 0,
                "process_write_pid_file failed!");
		
        return DFS_ERROR;
    }

    n = write(fd, buf, last - buf);

    close(fd);

    if (n < 0) 
	{
        return DFS_ERROR;
    }

    return DFS_OK;
}

int process_get_pid(cycle_t *cycle)
{
    int            n = 0;
    int            fd = -1;
    char           buf[11] = {0};
    uchar_t       *pid_file = NULL;
    conf_server_t *sconf  = (conf_server_t *)dfs_cycle->sconf;

    pid_file = sconf->pid_file.data;

    fd = dfs_sys_open(pid_file, O_RDWR , 0644);
    if (fd < 0) 
	{
        dfs_log_error(cycle->error_log,DFS_LOG_WARN, errno,
            "process_write_pid_file failed!");
		
        return DFS_ERROR;
    }

    n = read(fd, buf, 10);

    close(fd);

    if (n <= 0) 
	{
        return DFS_ERROR;
    }

    return atoi(buf);
}

void process_del_pid_file(void)
{
    uchar_t       *pid_file = NULL;
    conf_server_t *sconf = NULL;
    
    sconf = (conf_server_t *)dfs_cycle->sconf;
    pid_file = sconf->pid_file.data;
    
    unlink((char *)pid_file);
}

void process_close_other_channel()
{
    int i = 0;
	
    for (i = 0; i < process_last; i++) 
	{
        if (processes[i].pid == DFS_INVALID_PID) 
		{
            continue;
        }
		
        if (i == process_slot) 
		{
            continue;
        }
		
        if (processes[i].channel[1] == DFS_INVALID_FILE) 
		{
            continue;
        }
		
        if (close(processes[i].channel[1]) == DFS_ERROR) 
		{
            dfs_log_error(dfs_cycle->error_log, DFS_LOG_ALERT, errno,
                "close() channel failed");
        }
    }

    // close this process's write fd
    if (close(processes[process_slot].channel[0]) == DFS_ERROR) 
	{
        dfs_log_error(dfs_cycle->error_log, DFS_LOG_ALERT, errno,
            "close() channel failed");
    }
}

int process_quit_check()
{
    return (process_doing & PROCESS_DOING_QUIT) ||
        (process_doing & PROCESS_DOING_TERMINATE);
}

int process_run_check()
{
    return (!(process_doing & PROCESS_DOING_QUIT))
        && (!(process_doing & PROCESS_DOING_TERMINATE));
}

process_t * get_process(int slot)
{
    return &processes[slot];
}

void process_set_doing(int flag)
{
    process_doing |= flag;
}

int process_get_curslot() 
{
    return process_slot;
}

