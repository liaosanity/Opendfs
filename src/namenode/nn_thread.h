#ifndef NN_THREAD_H
#define NN_THREAD_H

#include "dfs_pipe.h"
#include "dfs_sys.h"
#include "dfs_conn_pool.h"
#include "dfs_queue.h"
#include "dfs_epoll.h"
#include "dfs_event_timer.h"
#include "dfs_notice.h"
#include "nn_cycle.h"
#include "nn_task_queue.h"

typedef void *(*TREAD_FUNC)(void *);
typedef struct dfs_thread_s dfs_thread_t;

struct dfs_thread_s 
{
    pthread_t      thread_id;
    int            type;
    event_base_t   event_base;
    event_timer_t  event_timer;
    conn_pool_t    conn_pool;
    task_queue_t   tq;
    task_queue_t  *bque;
    int            queue_size;
    notice_t       tq_notice;
    TREAD_FUNC     run_func;
    uint32_t       state;
    int            running;
};

enum 
{
    THREAD_MASTER,
    THREAD_DN,
    THREAD_CLI,
    THREAD_PAXOS,
    THREAD_TASK
};

enum 
{
    THREAD_ST_OK,
    THREAD_ST_EXIT,
    THREAD_ST_UNSTART,
};

void           thread_env_init();
dfs_thread_t  *thread_new(pool_t *pool);
void           thread_bind_key(dfs_thread_t *thread);
dfs_thread_t  *get_local_thread();
event_base_t  *thread_get_event_base();
event_timer_t *thread_get_event_timer();
conn_pool_t   *thread_get_conn_pool();
void           thread_clean(dfs_thread_t *thread);
int  thread_create(void *arg);
int  thread_event_init(dfs_thread_t *thread);
void thread_event_process(dfs_thread_t *thread);

#endif

