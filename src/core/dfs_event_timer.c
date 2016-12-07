#include "dfs_event_timer.h"
#include "dfs_error_log.h"

int event_timer_init(event_timer_t *timer, curtime_ptr handler, log_t *log)
{
    rbtree_init(&timer->timer_rbtree, &timer->timer_sentinel, 
		rbtree_insert_timer_value);
    timer->time_handler = handler;
    timer->log = log;
	
    return DFS_OK;
}

void event_timers_expire(event_timer_t *timer)
{
    event_t       *ev = NULL;
    rbtree_node_t *node = NULL;
    rbtree_node_t *root = NULL;
    rbtree_node_t *sentinel = NULL;

    for ( ;; ) 
	{
        sentinel = timer->timer_rbtree.sentinel;
        root = timer->timer_rbtree.root;

        if (root == sentinel) 
		{
            return;
        }

        node = rbtree_min(root, sentinel);

        if (node->key <= timer->time_handler()) 
		{
            ev = (event_t *) ((char *) node - offsetof(event_t, timer));

            rbtree_delete(&timer->timer_rbtree, &ev->timer);

            ev->timer_set = 0;
            ev->timedout = 1;

            ev->handler(ev);

            continue;
        }

        break;
    }
}

rb_msec_t event_find_timer(event_timer_t *ev_timer)
{
    rb_msec_int_t  timer = 0;
    rbtree_node_t *node = NULL;
    rbtree_node_t *root = NULL;
    rbtree_node_t *sentinel = NULL;

    if (ev_timer->timer_rbtree.root == &ev_timer->timer_sentinel)
	{
        return EVENT_TIMER_INFINITE;
    }

    root = ev_timer->timer_rbtree.root;
    sentinel = ev_timer->timer_rbtree.sentinel;
    node = rbtree_min(root, sentinel);

    timer = node->key - ev_timer->time_handler();

    return (timer > 0 ? timer : 0);
}

void event_timer_del(event_timer_t *ev_timer, event_t *ev)
{
    if (!ev->timer_set) 
	{
        return;
    }

    dfs_log_debug(ev_timer->log, DFS_LOG_DEBUG, 0, "delete timer: %p, event:%p",
        &ev->timer, ev);

    rbtree_delete(&ev_timer->timer_rbtree, &ev->timer);

    ev->timer_set = 0;
}

void event_timer_add(event_timer_t *ev_timer, event_t *ev, rb_msec_t timer)
{
    rb_msec_t     key;
    rb_msec_int_t diff;

    key = ev_timer->time_handler() + timer;
    if (ev->timer_set) 
	{
        /*
         * Use a previous timer value if difference between it and a new
         * value is less than EVENT_TIMER_LAZY_DELAY milliseconds: this allows
         * to minimize the rbtree operations for dfs connections.
         */
        diff = (rb_msec_int_t) (key - ev->timer.key);
        if (abs(diff) < EVENT_TIMER_LAZY_DELAY) 
		{
            return;
        }
		
        event_timer_del(ev_timer, ev);
    }

    ev->timer.key = key;

    rbtree_insert(&ev_timer->timer_rbtree, &ev->timer);

    dfs_log_debug(ev_timer->log, DFS_LOG_DEBUG, 0, "add timer: ev: %p, timer:%p",
        ev, &ev->timer);

    ev->timer_set = 1;
}

