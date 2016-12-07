#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "nn_conn_event.h"
#include "dfs_time.h"
#include "dfs_memory.h"
#include "dfs_event.h"
#include "dfs_epoll.h"
#include "dfs_lock.h"
#include "dfs_event_timer.h"
#include "dfs_memory.h"
#include "dfs_conn_pool.h"
#include "dfs_conn_listen.h"
#include "nn_conf.h"
#include "nn_request.h"
#include "nn_time.h"
#include "nn_process.h"

#define CONF_SERVER_UNLIMITED_ACCEPT_N 0
#define ADDR_MAX_LEN                   16

static void listen_rev_handler(event_t *ev);

int conn_listening_init(cycle_t *cycle)
{
    listening_t   *ls = NULL;
    conf_server_t *sconf = NULL;
    uint32_t       i = 0;
    server_bind_t *bind_for_cli = NULL;
    server_bind_t *bind_for_dn = NULL;
    
    sconf = (conf_server_t *)dfs_cycle->sconf;
	bind_for_dn = (server_bind_t *)sconf->bind_for_dn.elts;
	bind_for_cli = (server_bind_t *)sconf->bind_for_cli.elts;

	cycle->listening_for_dn.elts = pool_calloc(cycle->pool,
        sizeof(listening_t) * sconf->bind_for_dn.nelts);
    if (!cycle->listening_for_dn.elts) 
	{
         dfs_log_error(cycle->error_log, DFS_LOG_FATAL, 0,
            "no space to alloc listen_for_dn pool");
		 
        return DFS_ERROR;
    }

	cycle->listening_for_cli.elts = pool_calloc(cycle->pool,
        sizeof(listening_t) * sconf->bind_for_cli.nelts);
    if (!cycle->listening_for_cli.elts) 
	{
         dfs_log_error(cycle->error_log, DFS_LOG_FATAL, 0,
            "no space to alloc listen_for_cli pool");
		 
        return DFS_ERROR;
    }

	cycle->listening_for_dn.nelts = 0;
    cycle->listening_for_dn.size = sizeof(listening_t);
    cycle->listening_for_dn.nalloc = sconf->bind_for_dn.nelts;
    cycle->listening_for_dn.pool = cycle->pool;
	
    cycle->listening_for_cli.nelts = 0;
    cycle->listening_for_cli.size = sizeof(listening_t);
    cycle->listening_for_cli.nalloc = sconf->bind_for_cli.nelts;
    cycle->listening_for_cli.pool = cycle->pool;

	for (i = 0; i < sconf->bind_for_dn.nelts; i++) 
	{
        ls = conn_listening_add(&cycle->listening_for_dn, cycle->pool,
            cycle->error_log, inet_addr((char *)bind_for_dn[i].addr.data), 
            bind_for_dn[i].port, listen_rev_handler, 
            ((conf_server_t*)cycle->sconf)->recv_buff_len, 
            ((conf_server_t*)cycle->sconf)->recv_buff_len);
		
        if (!ls) 
		{
            return DFS_ERROR;
        }
    }

	for (i = 0; i < sconf->bind_for_cli.nelts; i++) 
	{
        ls = conn_listening_add(&cycle->listening_for_cli, cycle->pool,
            cycle->error_log, inet_addr((char *)bind_for_cli[i].addr.data), 
            bind_for_cli[i].port, listen_rev_handler, 
            ((conf_server_t*)cycle->sconf)->recv_buff_len, 
            ((conf_server_t*)cycle->sconf)->recv_buff_len);
		
        if (!ls) 
		{
            return DFS_ERROR;
        }
    }

	if (conn_listening_open(&cycle->listening_for_dn,
        cycle->error_log) != DFS_OK) 
    {
        return DFS_ERROR;
    }

    if (conn_listening_open(&cycle->listening_for_cli,
        cycle->error_log) != DFS_OK) 
    {
        return DFS_ERROR;
    }

    return DFS_OK;
}

static void listen_rev_handler(event_t *ev)
{
    int           s = DFS_INVALID_FILE;
    char          sa[DFS_SOCKLEN];
    log_t        *log = NULL;
    uchar_t      *address = NULL;
    conn_t       *lc = NULL;
    conn_t       *nc = NULL;
    event_t      *wev = NULL;
    socklen_t     socklen;
    listening_t  *ls = NULL;
    int           i = 0;
    conn_pool_t  *conn_pool = NULL;
    
    ev->ready = 0;
    lc = (conn_t *)ev->data;
    ls = lc->listening;
    
    errno = 0;
    socklen = DFS_SOCKLEN;
    conn_pool = thread_get_conn_pool();
    
    for (i = 0;
        ev->available == CONF_SERVER_UNLIMITED_ACCEPT_N || i < ev->available;
        i++) 
    {
        s = accept(lc->fd, (struct sockaddr *) sa, &socklen);

        if (s == DFS_INVALID_FILE) 
		{
            if (errno == DFS_EAGAIN) 
			{
                dfs_log_debug(dfs_cycle->error_log, DFS_LOG_DEBUG, errno,
                    "conn_accept: accept return EAGAIN");
            } 
			else if (errno == DFS_ECONNABORTED) 
			{
                dfs_log_debug(dfs_cycle->error_log, DFS_LOG_DEBUG, errno,
                    "conn_accept: accept client aborted connect");
            } 
			else 
			{
                dfs_log_error(dfs_cycle->error_log, DFS_LOG_WARN, errno,
                    "conn_accept: accept failed");
            }
			
            dfs_log_debug(dfs_cycle->error_log, DFS_LOG_DEBUG, 0,
                "conn_accept");
			
            return;
        }
      
        nc = conn_pool_get_connection(conn_pool);
        if (!nc) 
		{
            dfs_log_error(dfs_cycle->error_log, DFS_LOG_ALERT, 0,
                "conn_accept: get connection failed");
			
            close(s);
			
            return;
        }
		
        conn_set_default(nc, s);
       
        if (!nc->pool)
		{
            nc->pool = pool_create(ls->conn_psize, DEFAULT_PAGESIZE, 
				dfs_cycle->error_log);
            if (!nc->pool) 
			{
                dfs_log_error(dfs_cycle->error_log, DFS_LOG_ALERT, 0,
                    "conn_accept: create connection pool failed");
				
                goto error;
            }
        }
		
        nc->sockaddr = (sockaddr *)pool_alloc(nc->pool, socklen);
        if (!nc->sockaddr) 
		{
            dfs_log_error(dfs_cycle->error_log, DFS_LOG_ALERT, 0,
                "conn_accept: pool alloc sockaddr failed");
			
            goto error;
        }
		
        memory_memcpy(nc->sockaddr, sa, socklen);
		
        if (conn_nonblocking(s) == DFS_ERROR) 
		{
             dfs_log_error(dfs_cycle->error_log, DFS_LOG_ALERT, errno,
                "conn_accept: setnonblocking failed");
			 
             goto error;
        }
		
        log = dfs_cycle->error_log;
        nc->recv = dfs_recv;
        nc->send = dfs_send;
        nc->recv_chain = dfs_recv_chain;
        nc->send_chain = dfs_send_chain;
        nc->sendfile_chain = dfs_sendfile_chain;
        nc->log = log;
        nc->listening = ls;
        nc->socklen = socklen;
        wev = nc->write;
        wev->ready = DFS_FALSE;

        nc->addr_text.data = (uchar_t *)pool_calloc(nc->pool, ADDR_MAX_LEN);
        if (!nc->addr_text.data) 
		{
            dfs_log_error(dfs_cycle->error_log, DFS_LOG_ALERT, errno,
                "conn_accept: pool alloc addr_text failed");
			
            goto error;
        }
		
        address = (uchar_t *)inet_ntoa(((struct sockaddr_in *)
            nc->sockaddr)->sin_addr);
        if (address) 
		{
            nc->addr_text.len = string_strlen(address);
        }
		
        if (nc->addr_text.len == 0) 
		{
            dfs_log_error(dfs_cycle->error_log, DFS_LOG_ALERT, errno,
                "conn_accept: inet_ntoa server address failed");
			
            goto error;
        }
		
        memory_memcpy(nc->addr_text.data, address, nc->addr_text.len);

        dfs_log_debug(dfs_cycle->error_log, DFS_LOG_DEBUG, 0,
            "conn_accept: fd:%d conn:%p addr:%V, port:%d ls:%V",
            s, nc, &nc->addr_text,
            ntohs(((struct sockaddr_in *)nc->sockaddr)->sin_port), 
            &ls->addr_text);
		
        nc->accept_time = *time_timeofday();
        nn_conn_init(nc);
    }
		
error:
    conn_close(nc);
    conn_pool_free_connection(conn_pool, nc);
}

