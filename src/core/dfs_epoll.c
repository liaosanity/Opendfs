#include "dfs_epoll.h"
#include "dfs_memory.h"
#include "dfs_error_log.h"
#include "dfs_event.h"
#include "dfs_conn.h"

int epoll_init(event_base_t *ep_base, log_t *log)
{
    ep_base->ep = epoll_create(ep_base->nevents);
    if (ep_base->ep == DFS_INVALID_FILE) 
	{
        dfs_log_error(log, DFS_LOG_EMERG,
            errno, "epoll_init: epoll_create failed");
		
        return DFS_ERROR;
    }

    ep_base->event_list = (epoll_event_t *)memory_calloc(
        sizeof(struct epoll_event) * ep_base->nevents);
    if (!ep_base->event_list) 
	{
        dfs_log_error(log, DFS_LOG_EMERG, 0,
            "epoll_init: alloc event_list failed");
		
        return DFS_ERROR;
    }

#if (EVENT_HAVE_CLEAR_EVENT)
    ep_base->event_flags = EVENT_USE_CLEAR_EVENT
#else
    ep_base->event_flags = EVENT_USE_LEVEL_EVENT
#endif
        |EVENT_USE_GREEDY_EVENT
        |EVENT_USE_EPOLL_EVENT;

    queue_init(&ep_base->posted_accept_events);
    queue_init(&ep_base->posted_events);
    ep_base->log = log;
	
    return DFS_OK;
}

void epoll_done(event_base_t *ep_base)
{
    if (close(ep_base->ep) == DFS_ERROR) 
	{
        dfs_log_error(ep_base->log, DFS_LOG_ALERT, errno,
            "epoll close() failed");
    }

    ep_base->ep = DFS_INVALID_FILE;
    if (ep_base->event_list) 
	{
        memory_free(ep_base->event_list,
            sizeof(struct epoll_event) * ep_base->nevents);
        ep_base->event_list = NULL;
    }

    ep_base->nevents = 0;
    ep_base->event_flags = 0;
}

int epoll_add_event(event_base_t *ep_base, event_t *ev, 
	                      int event, uint32_t flags)
{
    int                 op = -1;
    conn_t             *c = NULL;
    event_t            *aevent = NULL;
    uint32_t            events = -1;
    uint32_t            aevents = -1;
    struct epoll_event  ee;
    
    memory_zero(&ee, sizeof(ee));
    c = (conn_t *)ev->data;
    events = (uint32_t) event;

    if (event == EVENT_READ_EVENT) 
	{
        aevent = c->write;
        aevents = EPOLLOUT;
    } 
	else 
	{
        aevent = c->read;
        aevents = EPOLLIN;
    }
	
    // another event is active, don't forget it
    if (aevent->active) 
	{
        op = EPOLL_CTL_MOD;
        events |= aevents;
    } 
	else 
	{
        op = EPOLL_CTL_ADD;
    }
	
    // flag is EPOLLET
    ee.events = events | (uint32_t) flags;
    ee.data.ptr = (void *) ((uintptr_t) c | ev->instance);

    ev->active = DFS_TRUE;

    if (epoll_ctl(ep_base->ep, op, c->fd, &ee) == -1) 
	{
        dfs_log_error(ep_base->log, DFS_LOG_ALERT, errno,
            "epoll_add_event: fd:%d op:%d, failed", c->fd, op);
        ev->active = DFS_FALSE;
		
        return DFS_ERROR;
    }

    //ev->active = DFS_TRUE;

    return DFS_OK;
}

int epoll_del_event(event_base_t *ep_base, event_t *ev, 
	                     int event, uint32_t flags)
{
    int                 op = -1;
    conn_t             *c = NULL;
    event_t            *e = NULL;
    uint32_t            prev = -1;
    struct epoll_event  ee;
    
    memory_zero(&ee, sizeof(ee));

    c = (conn_t *)ev->data;
 
    /*
     * when the file descriptor is closed, the epoll automatically deletes
     * it from its queue, so we do not need to delete explicity the event
     * before the closing the file descriptor
     */
    if (flags & EVENT_CLOSE_EVENT) 
	{
        ev->active = 0;
		
        return DFS_OK;
    }

    if (event == EVENT_READ_EVENT) 
	{
        e = c->write;
        prev = EPOLLOUT;
    } 
	else 
	{
        e = c->read;
        prev = EPOLLIN;
    }

    if (e->active) 
	{
        op = EPOLL_CTL_MOD;
        ee.events = prev | (uint32_t) flags;
        ee.data.ptr = (void *) ((uintptr_t) c | ev->instance);
    } 
	else 
	{
        op = EPOLL_CTL_DEL;
        //ee.events = 0;
        ee.events = event;
        ee.data.ptr = NULL;
    }

    if (epoll_ctl(ep_base->ep, op, c->fd, &ee) == -1) 
	{
        dfs_log_error(ep_base->log, DFS_LOG_ALERT, errno,
            "epoll_ctl(%d, %d) failed", op, c->fd);
		
        return DFS_ERROR;
    }

    ev->active = 0;
	
    return DFS_OK;
}

int epoll_add_connection(event_base_t *ep_base, conn_t *c)
{
    struct epoll_event ee;
    
    if (!c) 
	{
        return DFS_ERROR;
    }

    memory_zero(&ee, sizeof(ee));
    ee.events = EPOLLIN | EPOLLOUT | EPOLLET;
    ee.data.ptr = (void *) ((uintptr_t) c | c->read->instance);

    if (epoll_ctl(ep_base->ep, EPOLL_CTL_ADD, c->fd, &ee) == -1) 
	{
        dfs_log_error(ep_base->log, DFS_LOG_ALERT, errno,
            "epoll_ctl(EPOLL_CTL_ADD, %d) failed", c->fd);
		
        return DFS_ERROR;
    }

    c->read->active = DFS_TRUE;
    c->write->active = DFS_TRUE;

    return DFS_OK;
}

int epoll_del_connection(event_base_t *ep_base, conn_t *c, uint32_t flags)
{
    int                op;
    struct epoll_event ee;
 
    if (!ep_base) 
	{
        return DFS_OK;
    }
	
    /*
     * when the file descriptor is closed the epoll automatically deletes
     * it from its queue so we do not need to delete explicity the event
     * before the closing the file descriptor
     */
   
    if (flags & EVENT_CLOSE_EVENT) 
	{
        c->read->active = 0;
        c->write->active = 0;
        return DFS_OK;
    }

    op = EPOLL_CTL_DEL;
    ee.events = 0;
    ee.data.ptr = NULL;
	
    if (epoll_ctl(ep_base->ep, op, c->fd, &ee) == -1) 
	{
        dfs_log_error(ep_base->log, DFS_LOG_ALERT, errno,
            "epoll_ctl(%d, %d) failed", op, c->fd);
		
        return DFS_ERROR;
    }

    c->read->active = 0;
    c->write->active = 0;

    return DFS_OK;
}

int epoll_process_events(event_base_t *ep_base, rb_msec_t timer, 
	                             uint32_t flags)
{
    int               i = 0;
    int               hd_num = 0;
    int               events_num = 0;
    uint32_t          events = 0;
    int               instance = 0;
    conn_t           *c = NULL;
    event_t          *rev= NULL;
    event_t          *wev = NULL;
    volatile queue_t *queue = NULL;

    errno = 0;
    events_num = epoll_wait(ep_base->ep, ep_base->event_list,
        (int) ep_base->nevents, timer);

    if (flags & EVENT_UPDATE_TIME && ep_base->time_update) 
	{
        ep_base->time_update();
    }
	
    if (events_num == -1) 
	{
        dfs_log_error(ep_base->log, DFS_LOG_EMERG, errno,
            "epoll_process_events: epoll_wait failed");
		
        return DFS_ERROR;
    }
	
    if (events_num == 0) 
	{
        if (timer != EVENT_TIMER_INFINITE) 
		{
            return DFS_OK;
        }
		
        dfs_log_error(ep_base->log, DFS_LOG_ALERT, errno,
            "epoll_process_events: epoll_wait no events or timeout");
		
        return DFS_ERROR;
    }

    for (i = 0; i < events_num; i++) 
	{
        c = (conn_t *)ep_base->event_list[i].data.ptr;
        instance = (uintptr_t) c & 1;
        c = (conn_t *) ((uintptr_t) c & (uintptr_t) ~1);

        rev = c->read;
        if (c->fd == DFS_INVALID_FILE || rev->instance != instance) 
		{
            /*
             * the stale event from a file descriptor
             * that was just closed in this iteration
             */
            dfs_log_debug(ep_base->log, DFS_LOG_DEBUG, 0,
                "epoll_process_events: stale event %p", c);
			
            continue;
        }

        events = ep_base->event_list[i].events;
        if (events & (EPOLLERR|EPOLLHUP)) 
		{
            dfs_log_debug(ep_base->log, DFS_LOG_DEBUG, errno,
                "epoll_process_events: epoll_wait error on fd:%d ev:%ud",
                c->fd, events);
        }
		
        if ((events & (EPOLLERR|EPOLLHUP))
             && (events & (EPOLLIN|EPOLLOUT)) == 0) 
        {
            /*
             * if the error events were returned without
             * EPOLLIN or EPOLLOUT, then add these flags
             * to handle the events at least in one active handler
             */
            events |= EPOLLIN|EPOLLOUT;
        }

        if ((events & EPOLLIN) && rev->active) 
		{
            rev->ready = DFS_TRUE;
			
            if (!rev->handler) 
			{
                dfs_log_debug(ep_base->log, DFS_LOG_DEBUG, 0,
                    "epoll_process_events: rev->handler NULL");
				
                continue;
            }
			
            if (flags & EVENT_POST_EVENTS) 
			{
                queue = rev->accepted ? &ep_base->posted_accept_events:
                                        &ep_base->posted_events;
                rev->last_instance = instance;
				
                dfs_log_debug(ep_base->log, DFS_LOG_DEBUG, 0,
                    "epoll_process_events: post read event, fd:%d", c->fd);
				
                queue_insert_tail((queue_t*)queue, &rev->post_queue);
            } 
			else 
			{
                dfs_log_debug(ep_base->log, DFS_LOG_DEBUG, 0,
                    "epoll_process_events: read event fd:%d", c->fd);
				
                rev->handler(rev);
                hd_num++;
            }
        }

        wev = c->write;
		
        if ((events & EPOLLOUT) && wev->active) 
		{
            wev->ready = DFS_TRUE;
			
            if (!wev->handler) 
			{
                dfs_log_error(ep_base->log, DFS_LOG_WARN, 0,
                    "epoll_process_events: wev->handler NULL");
				
                continue;
            }
			
            if (flags & EVENT_POST_EVENTS) 
			{
                dfs_log_debug(ep_base->log, DFS_LOG_DEBUG, 0,
                    "epoll_process_events: post write event, fd:%d", c->fd);
				
                rev->last_instance = instance;
                queue_insert_tail((queue_t*)&ep_base->posted_events, &wev->post_queue);
            } 
			else 
			{
                dfs_log_debug(ep_base->log, DFS_LOG_DEBUG, 0,
                    "epoll_process_events: write event fd:%d", c->fd);
				
                wev->handler(wev);
                hd_num++;
            }
        }
    }

    return DFS_OK;
}

