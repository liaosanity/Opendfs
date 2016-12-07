#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if_arp.h>
#include <net/if.h>
#include <netinet/tcp.h>
#include <stdint.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "dfs_epoll.h"
#include "dfs_event_timer.h"
#include "dfs_memory.h"
#include "nn_thread.h"
#include "nn_worker_process.h"
#include "nn_net_response_handler.h"
#include "nn_request.h"
#include "nn_conf.h"

#define CONN_TIME_OUT        300000
#define TASK_TIME_OUT        100

#define NN_TASK_POOL_MAX_SIZE 64
#define NN_TASK_POOL_MIN_SIZE 8

extern dfs_thread_t  *dn_thread;
extern dfs_thread_t  *cli_thread;
task_t                busy_task;

static void nn_event_process_handler(event_t *ev);
//static void nn_empty_handler(event_t *ev);
static void nn_conn_read_handler(nn_conn_t *mc);
static void nn_conn_write_handler(nn_conn_t *mc);
static int nn_conn_out_buffer(conn_t *c, buffer_t *b);
static void nn_conn_free_queue(nn_conn_t *mc);
static void nn_conn_close(nn_conn_t *mc);
static int  nn_conn_recv(nn_conn_t *mc);
static int  nn_conn_decode(nn_conn_t *mc);

static void  nn_conn_timer_handler(event_t *ev)
{
    nn_conn_t *mc = (nn_conn_t *)ev->data;
	
    event_timer_del(mc->connection->ev_timer, ev);
    if (event_handle_read(mc->connection->ev_base, mc->connection->read, 0)) 
    {
        nn_conn_finalize(mc);
		
        return;
    }

    nn_conn_write_handler(mc);
    nn_conn_read_handler(mc);
}

void nn_conn_init(conn_t *c)
{
    event_t      *rev = NULL;
    event_t      *wev = NULL;
    nn_conn_t    *mc = NULL;
    pool_t       *pool = NULL;
    wb_node_t    *node = NULL;
    wb_node_t    *buff = NULL;
	dfs_thread_t *thread = NULL;
    int32_t       i = 0;
   
    thread = get_local_thread();
	
    pool = pool_create(4096, 4096, dfs_cycle->error_log);
    if (!pool) 
	{
        dfs_log_error(dfs_cycle->error_log,
             DFS_LOG_FATAL, 0, "pool create failed");
		
        goto error;
    }
	
    rev = c->read;
    wev = c->write;
    
    if (!c->conn_data) 
	{
        c->conn_data = pool_calloc(pool, sizeof(nn_conn_t));
    }
	
    mc = (nn_conn_t *)c->conn_data;
    if (!mc) 
	{
        dfs_log_error(dfs_cycle->error_log, DFS_LOG_ALERT, 0, 
			"mc create failed");
		
        goto error;
    }
	
    mc->mempool = pool;;
    mc->log = dfs_cycle->error_log;
    mc->in  = buffer_create(mc->mempool, 
   			 ((conf_server_t*)dfs_cycle->sconf)->recv_buff_len * 2);
    mc->out = buffer_create(mc->mempool,
			((conf_server_t*)dfs_cycle->sconf)->send_buff_len * 2);
    if (!mc->in || !mc->out) 
	{
        dfs_log_error(mc->log, DFS_LOG_ALERT, 0, "buffer_create failed");
		
        goto error;
    }
	
    c->ev_base = &thread->event_base;
    c->ev_timer = &thread->event_timer;
	mc->max_task = ((conf_server_t*)dfs_cycle->sconf)->max_tqueue_len;
    mc->count = 0;
    mc->connection = c;
    mc->slow = 0;
	
    snprintf(mc->ipaddr, sizeof(mc->ipaddr), "%s", c->addr_text.data);
	
    queue_init(&mc->free_task);
	
    buff = (wb_node_t *)pool_alloc(mc->mempool, mc->max_task * sizeof(wb_node_t));
    if (!buff) 
	{
        goto error;
    }
	
    for (i = 0; i < mc->max_task; i++) 
	{
        node = buff + i;
        node->qnode.tk.opq = &node->wbt;
		node->qnode.tk.data = NULL;
        (node->wbt).mc = mc;

		if (THREAD_DN == thread->type)
		{
			(node->wbt).thread = dn_thread;
		} 
		else if (THREAD_CLI == thread->type)
		{
			(node->wbt).thread = cli_thread;
		}
		
        queue_insert_head(&mc->free_task, &node->qnode.qe);
    }
	
    memset(&mc->ev_timer, 0, sizeof(event_t));
    mc->ev_timer.data = mc;
    mc->ev_timer.handler = nn_conn_timer_handler;
    rev->handler = nn_event_process_handler;
    wev->handler = nn_event_process_handler;
	
    queue_init(&mc->out_task);
	
    mc->read_event_handler = nn_conn_read_handler;
	
    nn_conn_update_state(mc, ST_CONNCECTED);
	
    if (event_handle_read(c->ev_base, rev, 0) == DFS_ERROR) 
	{
        dfs_log_error(mc->log, DFS_LOG_ALERT, 0, "add read event failed");
		
        goto error;
    }

    return;
	
error:
    if (mc && mc->mempool) 
	{
        pool_destroy(mc->mempool);
    }
    
    conn_release(c);
    conn_pool_free_connection(&thread->conn_pool, c);
}

static void nn_event_process_handler(event_t *ev)
{
    conn_t    *c = NULL;
    nn_conn_t *mc = NULL;
	
    c = (conn_t *)ev->data;
    mc = (nn_conn_t *)c->conn_data;

    if (ev->write) 
	{
        if (!mc->write_event_handler) 
		{
            dfs_log_error(mc->log, DFS_LOG_ERROR,
                0, "write handler NULL, fd:%d", c->fd);

            return;
        }

        mc->write_event_handler(mc);
    } 
	else 
	{
        if (!mc->read_event_handler) 
		{
            dfs_log_debug(mc->log, DFS_LOG_DEBUG,
                0, "read handler NULL, fd:%d", c->fd);

            return;
        }
		
        mc->read_event_handler(mc);
    }
}

//static void nn_empty_handler(event_t *ev)
//{   
//}

static int nn_conn_recv(nn_conn_t *mc)
{
    int     n = 0;
    conn_t *c = NULL;
    size_t  blen = 0;
	
	c = mc->connection;
	
    while (1) 
	{
		buffer_shrink(mc->in);
		
    	blen = buffer_free_size(mc->in);
	    if (!blen) 
		{
	        return DFS_BUFFER_FULL;
	    }
		
   		n = c->recv(c, mc->in->last, blen);
		if (n > 0) 
		{
			mc->in->last += n;
			
			continue;
		}
		
	    if (n == 0) 
		{
	        return DFS_CONN_CLOSED;
	    }
		
	    if (n == DFS_ERROR) 
		{
	        return DFS_ERROR;
	    }
		
	    if (n == DFS_AGAIN) 
		{
			return DFS_AGAIN;
	    }
	}
	
	return DFS_OK;
}

static int nn_conn_decode(nn_conn_t *mc)
{
    int                rc = 0;
    task_queue_node_t *node = NULL;
	
	while (1) 
	{
        node = (task_queue_node_t *)nn_conn_get_task(mc);
		if (!node) 
		{
            dfs_log_error(mc->log,DFS_LOG_ERROR, 0, 
				"mc to repiad remove from epoll");
   
			mc->slow = 1;
			event_del_read(mc->connection->ev_base, mc->connection->read);
            event_timer_add(mc->connection->ev_timer, &mc->ev_timer, 1000);
			
            return DFS_BUSY;
    	}	
			
        rc = task_decode(mc->in, &node->tk);
        if (rc == DFS_OK) 
		{
            dispatch_task(node);

            node = NULL;
			
            continue;
        }
		
        if (rc == DFS_ERROR) 
		{
			nn_conn_free_task(mc, &node->qe);
			buffer_reset(mc->in);
			
            return DFS_ERROR;
        }
            
        if (rc == DFS_AGAIN) 
		{
            nn_conn_free_task(mc, &node->qe);
			buffer_shrink(mc->in);
			
            return DFS_AGAIN;
        }
	}
   	
	return DFS_OK;
}

static void nn_conn_read_handler(nn_conn_t *mc)
{
	int   rc = 0;
	char *err = NULL;
	
    if (mc->state == ST_DISCONNCECTED) 
	{
        return;
    }
	
    rc = nn_conn_recv(mc);
    switch (rc) 
	{
    case DFS_CONN_CLOSED:
        err = (char *)"conn closed";
		
        goto error;
			
    case DFS_ERROR:
        err = (char *)"recv error, conn to be close";
		
        goto error;
			
    case DFS_AGAIN:
    case DFS_BUFFER_FULL:
        break;
    }
	
    rc = nn_conn_decode(mc);
    if (rc == DFS_ERROR) 
	{
    	err = (char *)"proto error";
	
    	goto error;
    }
	
   	return;
	
error:

    dfs_log_error(mc->log, DFS_LOG_ALERT, 0, err);
    nn_conn_finalize(mc);
}

static void nn_conn_write_handler(nn_conn_t *mc)
{
    conn_t *c = NULL;
	
    if (mc->state == ST_DISCONNCECTED) 
	{
        return;
    }
	
    c = mc->connection;
    
    if (c->write->timedout) 
	{
        dfs_log_error(mc->log, DFS_LOG_FATAL, 0,
                "conn_write timer out");
		
        nn_conn_finalize(mc);
		
        return;
    }
   
    if (c->write->timer_set) 
	{
        event_timer_del(c->ev_timer, c->write);
    }
   
	nn_conn_output(mc);  
}

void nn_conn_close(nn_conn_t *mc)
{
    conn_pool_t *pool = NULL;

    pool = thread_get_conn_pool();
    if (mc->connection) 
	{
        mc->connection->conn_data = NULL;
        conn_release(mc->connection);
        conn_pool_free_connection(pool, mc->connection);
        mc->connection = NULL;
    }
}

int nn_conn_is_connected(nn_conn_t *mc)
{
    return mc->state == ST_CONNCECTED;
}

static int nn_conn_out_buffer(conn_t *c, buffer_t *b)
{
    size_t size = 0;
    int    rc = 0;
    
    if (!c->write->ready) 
	{
        return DFS_AGAIN;
    }
  
    size = buffer_size(b);
	
    while (size) 
	{      
        rc = c->send(c, b->pos, size);
        if (rc < 0) 
		{
            return rc;
        }
        
        b->pos += rc,
        size -= rc;
    }
	
	buffer_reset(b);
    
    return DFS_OK;
}

int nn_conn_outtask(nn_conn_t *mc, task_t *t)
{   
    task_queue_node_t *node =NULL;
	node = queue_data(t, task_queue_node_t, tk);   
	
	if (mc->state != ST_CONNCECTED) 
	{
		nn_conn_free_task(mc, &node->qe);
        
		return DFS_OK;
	}

	queue_insert_tail(&mc->out_task, &node->qe);
    
	return nn_conn_output(mc);
}

int nn_conn_output(nn_conn_t *mc)
{
    conn_t            *c = NULL;
    int                rc =0;
	task_t            *t = NULL;
	task_queue_node_t *node = NULL;
	queue_t           *qe = NULL;
	char              *err_msg = NULL;              
    
    c = mc->connection;
    
    if (!c->write->ready && buffer_size(mc->out) > 0 ) 
	{
        return DFS_AGAIN;
    }
    
    mc->write_event_handler = nn_conn_write_handler;
    
repack:
	buffer_shrink(mc->out);
	
	while (!queue_empty(&mc->out_task)) 
	{
    	qe = queue_head(&mc->out_task);
		node = queue_data(qe, task_queue_node_t, qe);
		t =&node->tk;
		
		rc = task_encode(t, mc->out);
		if (rc == DFS_OK) 
		{
			queue_remove(qe);
			nn_conn_free_task(mc, qe);
			
			continue;
		}

		if (rc == DFS_AGAIN) 
		{
			goto send;
		}
		
		if (rc == DFS_ERROR) 
		{
            queue_remove(qe);
			nn_conn_free_task(mc, qe);
        }
	}

send:
    if(!buffer_size(mc->out)) 
	{
        return DFS_OK;
    }
    
	rc = nn_conn_out_buffer(c, mc->out);
    if (rc == DFS_ERROR) 
	{
        err_msg = (char *)"send data error  close conn";
		
		goto close;
    }
    
    if (rc == DFS_AGAIN) 
	{
        if (event_handle_write(c->ev_base, c->write, 0) == DFS_ERROR) 
		{
            dfs_log_error(mc->log, DFS_LOG_FATAL, 0, "event_handle_write");
			
        	return DFS_ERROR;
        }
		
        event_timer_add(c->ev_timer, c->write, CONN_TIME_OUT);
		
        return DFS_AGAIN; 
    }
    
    goto repack;
    
close:
	dfs_log_error(c->log, DFS_LOG_FATAL, 0, err_msg);
    nn_conn_finalize(mc);
	
	return DFS_ERROR;
}

void * nn_conn_get_task(nn_conn_t *mc)
{
    queue_t           *queue = NULL; 
    task_queue_node_t *node = NULL;
	
	if (mc->count >= mc->max_task)
	{
		dfs_log_error(mc->log,DFS_LOG_DEBUG, 0, 
			"get mc taskcount:%d", mc->count);
		
		return NULL;
	}
	
    queue = queue_head(&mc->free_task);
    node = queue_data(queue, task_queue_node_t, qe);
    queue_remove(queue);
    mc->count++;
	
    return node;  
}

void nn_conn_free_task(nn_conn_t *mc, queue_t *q)
{
   mc->count--;

   task_queue_node_t *node = queue_data(q, task_queue_node_t, qe);
   task_t *task = &node->tk;
   if (NULL != task->data && task->data_len > 0) 
   {
       free(task->data);
	   task->data = NULL;
   }
   
   queue_init(q);
   queue_insert_tail(&mc->free_task, q);
   
   if (mc->state == ST_DISCONNCECTED && mc->count == 0) 
   {
       nn_conn_finalize(mc);
   }
}

static void nn_conn_free_queue(nn_conn_t *mc)
{
    queue_t *qn = NULL;
	
    while (!queue_empty(&mc->out_task)) 
	{
        qn = queue_head(&mc->out_task);
        queue_remove(qn);
        nn_conn_free_task(mc, qn);
    }
}

int nn_conn_update_state(nn_conn_t *mc, int state)
{
    mc->state = state;
	
    return DFS_OK;
}

int nn_conn_get_state(nn_conn_t *mc)
{
    return mc->state;
}

void nn_conn_finalize(nn_conn_t *mc)
{
    if (mc->state == ST_CONNCECTED) 
	{
        if (mc->ev_timer.timer_set) 
		{
            event_timer_del(mc->connection->ev_timer,&mc->ev_timer);
        }
		
        nn_conn_close(mc);
        nn_conn_free_queue(mc);
        nn_conn_update_state(mc, ST_DISCONNCECTED);
    }

    if (mc->count> 0) 
	{
        return;
    }
	
    pool_destroy(mc->mempool);
}

