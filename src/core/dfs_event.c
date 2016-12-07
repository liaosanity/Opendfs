#include "dfs_event.h"
#include "dfs_conn.h"
#include "dfs_epoll.h"

void event_process_posted(volatile queue_t *posted, log_t *log)
{
    int      i = 0;
    conn_t  *c;
    event_t *ev = NULL;
    queue_t *eq = NULL;
	
    while (!queue_empty(posted)) 
	{
        eq = queue_head(posted);
		
        dfs_log_debug(log, DFS_LOG_DEBUG, 0,
            "event_process_posted: posted %l, eq %l eq->prev %l "
            "eq->next %l index %d", (size_t)posted, (size_t)eq,
            (size_t)eq->prev, (size_t)eq->next, i);
		
        queue_remove(eq);
		
        ev = queue_data(eq, event_t, post_queue);
        if (!ev) 
		{
            dfs_log_debug(log, DFS_LOG_DEBUG, 0,
                "event_process_posted: ev is NULL");
			
            return;
        }

        c = (conn_t *)ev->data;
		
        dfs_log_debug(log, DFS_LOG_DEBUG, 0,
            "event_process_posted: fd:%d conn:%p, timer key:%M write:%d",
            c->fd, c, ev->timer.key, ev->write);

        if (c->fd != DFS_INVALID_FILE) 
		{
            dfs_log_debug(log, DFS_LOG_DEBUG, 0,
                "%s: fd:%d conn:%p, timer key:%M write:%d, handle %p", __func__,
                c->fd, c, ev->timer.key, ev->write, ev->handler);

            ev->handler(ev);

        } 
		else 
		{
            dfs_log_debug(log, DFS_LOG_DEBUG, 0,
                "event_process_posted: stale event conn:%p, fd:%d", c, c->fd);
        }
		
        i++;
    }
}

int event_handle_read(event_base_t *base, event_t *rev, uint32_t flags)
{
    if (!rev->active && !rev->ready) 
	{
        if (event_add(base, rev, EVENT_READ_EVENT,
            EVENT_CLEAR_EVENT) == DFS_ERROR)
        {
            return DFS_ERROR;
        }
    }

    return DFS_OK;
}

int event_del_read(event_base_t *base, event_t *rev)
{
    rev->ready = DFS_FALSE;
	
    return event_delete(base, rev, EVENT_READ_EVENT, EVENT_CLEAR_EVENT);
}

int event_handle_write(event_base_t *base, event_t *wev, size_t lowat)
{
    if (!wev->active && !wev->ready) 
	{
        if (event_add(base, wev, EVENT_WRITE_EVENT,
            EVENT_CLEAR_EVENT) == DFS_ERROR) 
        {
            return DFS_ERROR;
        }
    } 
	else 
	{
        dfs_log_debug(base->log,DFS_LOG_DEBUG, 0,
            "%s: fd:%d already in epoll", __func__, event_fd(wev->data));
    }

    return DFS_OK;
}

int event_del_write(event_base_t *base, event_t *wev)
{
    wev->ready = DFS_FALSE;
	
    return event_delete(base, wev, EVENT_WRITE_EVENT, EVENT_CLEAR_EVENT);
}

