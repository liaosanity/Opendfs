#include "dn_thread.h"
#include "dfs_memory.h"
#include "dfs_sys.h"
#include "dfs_conn_listen.h"
#include "dn_time.h"
#include "dn_process.h"

extern _xvolatile rb_msec_t dfs_current_msec;
static pthread_key_t dfs_thread_key;
dfs_atomic_lock_t accept_lock;
volatile pthread_t accept_lock_held = -1;
int accecpt_enable = DFS_TRUE;

extern uint32_t  process_doing;
extern process_t processes[];
extern int       process_slot;

static int event_trylock_accept_lock(dfs_thread_t *thread);
static int event_free_accept_lock(dfs_thread_t *thread);

void thread_env_init()
{
    pthread_key_create(&dfs_thread_key, NULL);
}

dfs_thread_t *thread_new(pool_t *pool)
{
    if (pool) 
	{
        return (dfs_thread_t *)pool_calloc(pool, sizeof(dfs_thread_t));
    }
	
    return (dfs_thread_t *)memory_calloc(sizeof(dfs_thread_t));
}

void thread_bind_key(dfs_thread_t *thread)
{
    pthread_setspecific(dfs_thread_key, thread);
}

dfs_thread_t *get_local_thread()
{
    return (dfs_thread_t *)pthread_getspecific(dfs_thread_key);
}

event_base_t *thread_get_event_base()
{
    dfs_thread_t *thread = (dfs_thread_t *)pthread_getspecific(dfs_thread_key);
	
    return thread != NULL ? &thread->event_base : NULL;
}

event_timer_t *thread_get_event_timer()
{
    dfs_thread_t *thread = (dfs_thread_t *)pthread_getspecific(dfs_thread_key);
	
    return thread != NULL ? &thread->event_timer : NULL;
}

conn_pool_t * thread_get_conn_pool()
{
    dfs_thread_t *thread = (dfs_thread_t *)pthread_getspecific(dfs_thread_key);
	
    return &thread->conn_pool;
}

int thread_create(void *args)
{
    pthread_attr_t  attr;
    int             ret;
    dfs_thread_t   *thread = (dfs_thread_t *)args;

    pthread_attr_init(&attr);
    
    if ((ret = pthread_create(&thread->thread_id, &attr, 
		thread->run_func, thread)) != 0) 
    {
        dfs_log_error(dfs_cycle->error_log, DFS_LOG_FATAL, 0,
            "thread_create err: %s\n", strerror(ret));
		
        return DFS_ERROR;
    }

    return DFS_OK;
}

void thread_clean(dfs_thread_t *thread)
{
}

int thread_event_init(dfs_thread_t *thread)
{
    io_event_t *ioevents = NULL;
	
    if (epoll_init(&thread->event_base, dfs_cycle->error_log) == DFS_ERROR) 
	{
        return DFS_ERROR;
    }

    event_timer_init(&thread->event_timer, time_curtime, dfs_cycle->error_log);

	queue_init((queue_t *)&thread->posted_accept_events);
    queue_init((queue_t *)&thread->posted_events);

	ioevents = &thread->io_events;
    ioevents->lock.lock = DFS_LOCK_OFF;
    ioevents->lock.allocator = NULL;
    
    ioevents->bad_lock.lock = DFS_LOCK_OFF;
    ioevents->bad_lock.allocator = NULL;

    return DFS_OK;
}

void thread_event_process(dfs_thread_t *thread)
{
    uint32_t      flags = EVENT_UPDATE_TIME;
    rb_msec_t     timer = 0;
    rb_msec_t     delta = 0;
    event_base_t *ev_base = NULL;
	array_t      *listens = NULL;
    
    ev_base = &thread->event_base;
	listens = cycle_get_listen_for_cli();

	if ((!(process_doing & PROCESS_DOING_QUIT))
        && (!(process_doing & PROCESS_DOING_TERMINATE))) 
    {
        if (thread->type == THREAD_WORKER 
			&& event_trylock_accept_lock(thread)) 
		{
        }
    }

	if (accecpt_enable && accept_lock_held == thread->thread_id &&
        ((process_doing & PROCESS_DOING_QUIT) ||
        (process_doing & PROCESS_DOING_TERMINATE))) 
    {
        conn_listening_del_event(ev_base, listens);
        accecpt_enable = DFS_FALSE;
        event_free_accept_lock(thread);
    }

	if (accept_lock_held == thread->thread_id
        && conn_listening_add_event(ev_base, listens) == DFS_OK) 
    {
        flags |= EVENT_POST_EVENTS;
    } 
	else 
    {
        event_free_accept_lock(thread);
    }
    
    timer = event_find_timer(&thread->event_timer);

    if ((timer > 10) || (timer == EVENT_TIMER_INFINITE)) 
	{
        timer = 10;
    }
    
    delta = dfs_current_msec;

    (void) event_process_events(ev_base, timer, flags);

    if ((THREAD_WORKER == thread->type) 
		&& !queue_empty(&ev_base->posted_accept_events)) 
    {
        event_process_posted(&ev_base->posted_accept_events, ev_base->log);
    }

	if (accept_lock_held == thread->thread_id) 
	{
        conn_listening_del_event(ev_base, listens);
        event_free_accept_lock(thread);
    }

    if ((THREAD_WORKER == thread->type) 
		&& !queue_empty(&ev_base->posted_events)) 
    {
        event_process_posted(&ev_base->posted_events, ev_base->log);
    }

    delta = dfs_current_msec - delta;
    if (delta) 
	{
        event_timers_expire(&thread->event_timer);
    }

	if (THREAD_WORKER == thread->type) 
	{
		cfs_ioevents_process_posted(&thread->io_events, &thread->fio_mgr);
    }
}

void accept_lock_init()
{
    accept_lock.lock = DFS_LOCK_OFF;
    accept_lock.allocator = NULL;
}

static int event_trylock_accept_lock(dfs_thread_t *thread)
{
    dfs_lock_errno_t flerrno;

    if (dfs_atomic_lock_try_on(&accept_lock, &flerrno) == DFS_LOCK_ON)
	{
        accept_lock_held = thread->thread_id;
    }
    
    return DFS_OK;
}

static int event_free_accept_lock(dfs_thread_t *thread)
{
    dfs_lock_errno_t flerrno;
    
    if (accept_lock_held == -1U || accept_lock_held != thread->thread_id) 
	{
        return DFS_OK;
    }
    
    accept_lock_held = -1U;

    dfs_atomic_lock_off(&accept_lock, &flerrno);

    return DFS_OK;
}


