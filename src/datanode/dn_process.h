#ifndef DN_PROCESS_H
#define DN_PROCESS_H

#include "dfs_types.h"
#include "dfs_string.h"
#include "dn_cycle.h"

#define PROCESSES_MAX        1024
#define PROCESS_SLOT_AUTO   -1
#define PROCESS_FATAL_EXIT   2
#define PROCESS_KILL_EXIT    3
#define MAX_RESTART_NUM      1


typedef void (*spawn_proc_pt) (cycle_t *cycle, void *data);

typedef struct 
{
    pid_t                pid;
    int                  status;
    int                  channel[2];
    spawn_proc_pt        proc;
    void                *data;
    char                *name;
    uint32_t             ps;          // process status
    uint32_t             restart_gap; // restart num
    uint32_t             ow;          // old worker flag;
    pthread_mutex_t      lock;
    pthread_mutexattr_t  mutex_attr;
} process_t;

/*
 * the actions that process will do,
 * come from signal handler
 */
enum 
{
    PROCESS_DOING_REAP          = 0x0001,
    PROCESS_DOING_TERMINATE     = 0x0002,
    PROCESS_DOING_QUIT          = 0x0004,
    PROCESS_DOING_RECONF        = 0x0008,
    PROCESS_DOING_ALARM         = 0x0010,
    PROCESS_DOING_RECOVERY      = 0x0100,
    PROCESS_DOING_TEST_STORE    = 0x0200,
    PROCESS_DOING_REOPEN        = 0x0400,
    PROCESS_DOING_BACKUP        = 0X0800,
};

enum 
{
    PROCESS_STATUS_RUNNING      = 0x0001, // after start, running status
    PROCESS_STATUS_EXITING      = 0x0002,
    PROCESS_STATUS_EXITED       = 0x0004,
    PROCESS_STATUS_NORESPAWN    = 0x0008  // exited, but don't restart
};

enum 
{
    PROCESS_MASTER,
    PROCESS_WORKER,
    PROCESS_HELPER
};

void process_set_title(string_t *title);
int  process_change_workdir(string_t *dir);
void process_master_cycle(cycle_t *cycle, int argc, char **argv);
int  process_write_pid_file(pid_t pid);
void process_del_pid_file(void);
int  process_get_pid(cycle_t *cycle);
int  process_check_running(cycle_t *cycle);
void process_close_other_channel();
int  process_quit_check();
int  process_run_check();
process_t* get_process(int slot);
void process_set_doing(int flag);
int process_get_curslot();

#endif

