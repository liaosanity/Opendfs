#include "dfs_notice.h"
#include "dfs_conn.h"

#define NOTICE_BUFF_SIZE 1024

static void noice_read_event_handler(event_t *ev);

int notice_init(event_base_t *base, notice_t *n, 
	              wake_up_hander handler, void *data)
{
    conn_t  *c = NULL;
    event_t *rev = NULL;
    
    if (pipe_open(&n->channel) != DFS_OK) 
	{
        return DFS_ERROR;
    }
    
    c = conn_get_from_mem(n->channel.pfd[0]);
    if (!c) 
	{
        goto error;
    }

    conn_nonblocking(n->channel.pfd[0]);
    conn_nonblocking(n->channel.pfd[1]);
    n->call_back = handler;
    n->data = data;
    n->wake_up = notice_wake_up;
    
    c->ev_base = base;
    c->conn_data = n;
    c->log = base->log;
    
    rev = c->read;
    rev->handler = noice_read_event_handler;
    
    if (event_add(base,rev, EVENT_READ_EVENT, 0) == DFS_ERROR) 
	{
        dfs_log_error(base->log, DFS_LOG_FATAL, 0,"event adde failed");
		
        goto error;
    }
    
    n->log = base->log;

    return DFS_OK;
    
error:
    pipe_close(&n->channel);
    
    if (c) 
	{
        conn_free_mem(c);
    }
    
    return DFS_ERROR;
}

int notice_wake_up(notice_t *n)
{
    if (dfs_write_fd(n->channel.pfd[1], "C", 1) == DFS_ERROR) 
	{
        if (errno != DFS_EAGAIN) 
		{
            dfs_log_error(n->log, DFS_LOG_FATAL, 0, "notice wake up failed %d",
                errno);
        }
    }
    
    return DFS_OK;
}

static void noice_read_event_handler(event_t *ev)
{
    conn_t   *c = NULL;
    notice_t *nt = NULL;
    uchar_t   buff[NOTICE_BUFF_SIZE];
    int       n = 0;
    char     *errmsg = NULL;
    
    c = (conn_t *)ev->data;
    nt = (notice_t *)c->conn_data;
    
    while (1) 
	{
        n = dfs_read_fd(nt->channel.pfd[0], buff, NOTICE_BUFF_SIZE);
        if (n > 0 || (n < 0 && errno == DFS_EINTR)) 
		{
            continue;
        }

        if (n == 0) 
		{
            errmsg = (char *)"read 0 byte!";
        } 
		else if (errno != DFS_EAGAIN) 
		{
            errmsg = (char *)"notice read failed";
        }
        
        break;
    }

    if (errmsg) 
	{
        dfs_log_error(nt->log, DFS_LOG_FATAL, 0,
                "pipe fd[%d] %s", nt->channel.pfd[0], errmsg);
    }
	
    nt->call_back(nt->data);
}

