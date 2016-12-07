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
#include "dn_request.h"
#include "dn_thread.h"
#include "dn_data_storage.h"
#include "dn_conf.h"

static void dn_empty_handler(event_t *ev);
static void dn_request_process_handler(event_t *ev);
static void dn_request_read_header(dn_request_t *r);
static void dn_request_close(dn_request_t *r, uint32_t err);
static void dn_request_parse_header(dn_request_t *r);
static void dn_request_block_reading(dn_request_t *r);
static void dn_request_block_writing(dn_request_t *r);
static void dn_request_read_file(dn_request_t *r);
static void dn_request_write_file(dn_request_t *r);
static void dn_request_header_response(dn_request_t *r);
static void dn_request_send_header_response(dn_request_t *r);
static void dn_request_check_connection(dn_request_t *r, 
	event_t *ev);
static void dn_request_check_read_connection(dn_request_t *r);
static void dn_request_check_write_connection(dn_request_t *r);
static int send_header_response(dn_request_t *r);
static void dn_request_process_body(dn_request_t *r);
static void dn_request_send_block(dn_request_t *r);
static void fio_task_alloc_timeout(event_t *ev);
static int block_read_complete(void *data, void *task);
static void dn_request_send_block_again(dn_request_t *r);
static void dn_request_recv_block(dn_request_t *r);
static void recv_block_handler(dn_request_t *r);
static int block_write_complete(void *data, void *task);
static void dn_request_write_done_response(dn_request_t *r);
static void dn_request_send_write_done_response(dn_request_t *r);
static void dn_request_read_done_response(dn_request_t *r);
static void dn_request_send_read_done_response(dn_request_t *r);

void dn_conn_init(conn_t *c)
{
    event_t       *rev = NULL;
    event_t       *wev = NULL;
    dn_request_t  *r = NULL;
    dfs_thread_t  *thread = NULL;

	thread = get_local_thread();

	rev = c->read;
	rev->handler = dn_request_init;

	wev = c->write;
	wev->handler = dn_empty_handler;

	if (!c->conn_data) 
    {
        c->conn_data = pool_calloc(c->pool, sizeof(dn_request_t));
		if (!c->conn_data) 
		{
            dfs_log_error(dfs_cycle->error_log, DFS_LOG_ALERT, 0, 
				"pool_calloc failed");

			conn_release(c);
			conn_pool_free_connection(&thread->conn_pool, c);
			
            return;
		}
    }

	r = (dn_request_t *)c->conn_data;
	r->conn = c;
	memset(&r->header, 0x00, sizeof(data_transfer_header_t));
	r->store_fd = -1;

	r->pool = pool_create(CONN_POOL_SZ, CONN_POOL_SZ, dfs_cycle->error_log);
    if (!r->pool) 
	{
        dfs_log_error(dfs_cycle->error_log,
             DFS_LOG_FATAL, 0, "pool_create failed");
		
        dn_request_close(r, DN_STATUS_INTERNAL_SERVER_ERROR);
			
        return;
    }

	snprintf(r->ipaddr, sizeof(r->ipaddr), "%s", c->addr_text.data);

	c->ev_base = &thread->event_base;
	c->ev_timer = &thread->event_timer;

	if (event_handle_read(c->ev_base, rev, 0) == DFS_ERROR) 
	{
        dfs_log_error(dfs_cycle->error_log, DFS_LOG_ALERT, 0, 
			"add read event failed");
		
        dn_request_close(r, DN_STATUS_INTERNAL_SERVER_ERROR);
    }
}

static void dn_empty_handler(event_t *ev)
{
    if (ev->write) 
	{
        dfs_log_debug(dfs_cycle->error_log, DFS_LOG_DEBUG, 0, 
			"dn_empty_handler write event");
    } 
	else 
	{
        dfs_log_debug(dfs_cycle->error_log, DFS_LOG_DEBUG, 0, 
			"dn_empty_handler read event");
    }
}

void dn_request_init(event_t *rev)
{
    conn_t       *c = NULL;
    dn_request_t *r = NULL;
    event_t      *wev = NULL;

    c = (conn_t *)rev->data;
	r = (dn_request_t *)c->conn_data;
	wev = c->write;

	rev->handler = dn_request_process_handler;
    wev->handler = dn_request_process_handler;

    r->read_event_handler = dn_request_read_header;

	dn_request_read_header(r);
}

static void dn_request_process_handler(event_t *ev)
{
    conn_t       *c = NULL;
    dn_request_t *r = NULL;

	c = (conn_t *)ev->data;
	r = (dn_request_t *)c->conn_data;

	if (ev->write) 
	{
        if (!r->write_event_handler) 
		{
            dfs_log_debug(dfs_cycle->error_log, DFS_LOG_DEBUG, 0, 
				"write handler NULL, conn_fd: %d", c->fd);
			
            return;
        }
		
        r->write_event_handler(r);
    } 
	else 
	{
        if (!r->read_event_handler) 
		{
            dfs_log_debug(dfs_cycle->error_log, DFS_LOG_DEBUG, 0, 
				"read handler NULL, conn_fd: %d", c->fd);
			
            return;
        }
		
        r->read_event_handler(r);
    }
}

static void dn_request_read_header(dn_request_t *r)
{
    conn_t   *c = NULL;
	event_t  *rev = NULL;
	ssize_t   rs = 0;

	c = r->conn;
	rev = c->read;

	if (rev->timedout) 
	{
        dfs_log_debug(dfs_cycle->error_log, DFS_LOG_DEBUG, 0, 
			"dn_request_read_header, rev timeout conn_fd: %d", c->fd);
		
        dn_request_close(r, DN_REQUEST_ERROR_TIMEOUT);
		
        return;
    }

	if (rev->timer_set) 
	{
        event_timer_del(c->ev_timer, rev);
    }

	if (rev->ready) 
	{
        rs = c->recv(c, (uchar_t *)&r->header, sizeof(data_transfer_header_t));
    } 
	else 
	{
        rs = DFS_AGAIN;
    }

	if (rs > 0) 
	{
		dn_request_parse_header(r);
    }
    else if (rs <= 0) 
	{
        if (rs == DFS_AGAIN) 
		{
            event_timer_add(c->ev_timer, rev, CONN_TIME_OUT);
		
            rev->ready = DFS_FALSE;
        }
		else 
		{
            dfs_log_debug(dfs_cycle->error_log, DFS_LOG_DEBUG, 0, 
				"dn_request_read_header, read header err, conn_fd: %d", c->fd);

			dn_request_close(r, DN_REQUEST_ERROR_READ_REQUEST);
		}
    }
}

static void dn_request_close(dn_request_t *r, uint32_t err)
{
    conn_t       *c = NULL;
	dfs_thread_t *thread = NULL;

	dfs_log_debug(dfs_cycle->error_log, DFS_LOG_DEBUG, 0, 
		"dn_request_close err: %d", err);

	c = r->conn;
	thread = get_local_thread();

	if (r->fio) 
	{
        cfs_fio_manager_free(r->fio, &thread->fio_mgr);
		r->fio = NULL;
	}

	if (r->store_fd > 0) 
	{
        cfs_close((cfs_t *)dfs_cycle->cfs, r->store_fd);
		r->store_fd = -1;
	}

	if (r->pool) 
	{
        pool_destroy(r->pool);
		r->pool = NULL;
    }
	
    conn_release(c);
    conn_pool_free_connection(&thread->conn_pool, c);
}

static void dn_request_parse_header(dn_request_t *r)
{
    int op_type = r->header.op_type;
	
    r->read_event_handler = dn_request_block_reading;
	
    switch (op_type) 
	{
	case OP_WRITE_BLOCK:
		dn_request_write_file(r);
		break;
		
	case OP_READ_BLOCK:
		dn_request_read_file(r);
		break;
		
	default:
		dfs_log_error(dfs_cycle->error_log, DFS_LOG_ALERT, 0, 
			"unknown op_type: %d", op_type);

		dn_request_close(r, DN_REQUEST_ERROR_CONN);
		
		return;
	}
}

static void dn_request_block_reading(dn_request_t *r)
{
    conn_t  *c = NULL;
	event_t *rev = NULL;

	c = r->conn;
	rev = c->read;
	
    if (event_delete(c->ev_base, rev, EVENT_READ_EVENT, EVENT_CLEAR_EVENT) 
		== DFS_ERROR)
	{
	    dfs_log_error(dfs_cycle->error_log, DFS_LOG_ALERT, 0, 
			"del read event failed");
		
        dn_request_close(r, DN_REQUEST_ERROR_CONN);
    }
}

static void dn_request_block_writing(dn_request_t *r)
{
    conn_t  *c = NULL;
	event_t *wev = NULL;

	c = r->conn;
	wev = c->read;
	
    if (event_delete(c->ev_base, wev, EVENT_WRITE_EVENT, EVENT_CLEAR_EVENT) 
		== DFS_ERROR)
	{
	    dfs_log_error(dfs_cycle->error_log, DFS_LOG_ALERT, 0, 
			"del write event failed");
		
        dn_request_close(r, DN_REQUEST_ERROR_CONN);
    }
}

static void dn_request_read_file(dn_request_t *r)
{
    block_info_t *blk = NULL;
	int           fd = -1;

	blk = block_object_get(r->header.block_id);
	if (!blk) 
	{
        dfs_log_error(dfs_cycle->error_log,
             DFS_LOG_FATAL, 0, "blk %d does't exist", r->header.block_id);

        dn_request_close(r, DN_REQUEST_ERROR_BLK_NO_EXIST);

		return;
	}

	if (r->store_fd < 0) 
	{
        fd = cfs_open((cfs_t *)dfs_cycle->cfs, (uchar_t *)blk->path, O_RDONLY, 
			dfs_cycle->error_log);
		if (fd < 0) 
		{
		    dfs_log_error(dfs_cycle->error_log, 
				DFS_LOG_FATAL, errno, "open file %s err", blk->path);
			
		    dn_request_close(r, DN_STATUS_INTERNAL_SERVER_ERROR);
			
            return;
		}

		r->store_fd = fd;
	}

	dn_request_header_response(r);
}

static void dn_request_write_file(dn_request_t *r)
{
    conf_server_t *sconf = NULL;
	int            fd = -1;

	sconf = (conf_server_t*)dfs_cycle->sconf;
	
    r->input = buffer_create(r->pool, sconf->recv_buff_len * 2);
	if (!r->input) 
	{
        dfs_log_error(dfs_cycle->error_log, DFS_LOG_ALERT, 0, 
			"buffer_create failed");

		dn_request_close(r, DN_STATUS_INTERNAL_SERVER_ERROR);

		return;
	}

	if (get_block_temp_path(r) != DFS_OK) 
	{
		dn_request_close(r, DN_STATUS_INTERNAL_SERVER_ERROR);

		return;
	}

	if (r->store_fd < 0) 
	{
        fd = cfs_open((cfs_t *)dfs_cycle->cfs, r->path, 
			O_CREAT | O_WRONLY | O_TRUNC, dfs_cycle->error_log);
		if (fd < 0) 
		{
		    dfs_log_error(dfs_cycle->error_log, 
				DFS_LOG_FATAL, errno, "open file %s err", r->path);
			
		    dn_request_close(r, DN_STATUS_INTERNAL_SERVER_ERROR);
			
            return;
		}

		r->store_fd = fd;
	}

	dn_request_header_response(r);
}

static void dn_request_header_response(dn_request_t *r)
{
    data_transfer_header_rsp_t  header_rsp;
	chain_t                    *out = NULL;
	buffer_t                   *b = NULL;
	conn_t                     *c = NULL;
    int                         header_sz = 0;

	header_rsp.op_status = OP_STATUS_SUCCESS;
	header_rsp.err = DFS_OK;
	
	c = r->conn;
	header_sz = sizeof(data_transfer_header_rsp_t);

	out = chain_alloc(r->pool);
	if (!out) 
	{
	    dfs_log_error(dfs_cycle->error_log, DFS_LOG_ALERT, 0, 
			"chain_alloc failed");

		dn_request_close(r, DN_STATUS_INTERNAL_SERVER_ERROR);

		return;
	}

	b = buffer_create(r->pool, header_sz);
	if (!b) 
	{
	    dfs_log_error(dfs_cycle->error_log, DFS_LOG_ALERT, 0, 
			"buffer_create failed");

		dn_request_close(r, DN_STATUS_INTERNAL_SERVER_ERROR);

		return;
	}

	out->buf = b;
	b->last = memory_cpymem(b->last, &header_rsp, header_sz);

    if (!r->output) 
	{
        r->output = (chain_output_ctx_t *)pool_alloc(r->pool, 
			sizeof(chain_output_ctx_t));
		if (!r->output) 
		{
            dfs_log_error(dfs_cycle->error_log, DFS_LOG_ALERT, 0, 
				"pool_alloc failed");

		    dn_request_close(r, DN_STATUS_INTERNAL_SERVER_ERROR);

		    return;
		}
	}

	r->output->out = NULL;
	chain_append_all(&r->output->out, out);

	r->write_event_handler = dn_request_send_header_response;
    r->read_event_handler = dn_request_check_read_connection;

	if (c->write->ready) 
	{
        dn_request_send_header_response(r);
		
        return;
    }

	if (event_handle_write(c->ev_base, c->write, 0) == DFS_ERROR) 
	{
        dfs_log_error(dfs_cycle->error_log, DFS_LOG_ALERT, 0, 
			"add write event failed");
        
        dn_request_close(r, DN_REQUEST_ERROR_SPECIAL_RESPONSE);
		
        return;
    }
    
    event_timer_add(c->ev_timer, c->write, CONN_TIME_OUT);
}

static void dn_request_send_header_response(dn_request_t *r)
{
    conn_t  *c = NULL;
	event_t *wev = NULL;
	int      rs = 0;

	c = r->conn;
	wev = c->write;

	if (wev->timedout) 
	{
	    dfs_log_debug(dfs_cycle->error_log, DFS_LOG_DEBUG, 0, 
			"dn_request_send_header_response, wev timeout, conn_fd: %d", c->fd);
		
		dn_request_close(r, DN_REQUEST_ERROR_CONN);

		return;
    }

	if (wev->timer_set) 
	{
        event_timer_del(c->ev_timer, wev);
    }

	rs = send_header_response(r);
	if (rs == DFS_OK) 
	{
		dn_request_process_body(r);
		
	    return;
	}
	else if (rs == DFS_AGAIN) 
	{
        event_timer_add(c->ev_timer, wev, CONN_TIME_OUT);
		
        return;
    }

	dn_request_close(r, DN_REQUEST_ERROR_SPECIAL_RESPONSE);
}

static void dn_request_check_read_connection(dn_request_t *r)
{
    dn_request_check_connection(r, r->conn->read);
}

static void dn_request_check_write_connection(dn_request_t *r)
{
    dn_request_check_connection(r, r->conn->write);
}

static void dn_request_check_connection(dn_request_t *r, 
	event_t *ev)
{
    conn_t *c = NULL;
	char    buf[1] = "";
	int     rs = 0;

	c = r->conn;
	ev = c->read;

	if (ev->timedout) 
	{
        dfs_log_debug(dfs_cycle->error_log, DFS_LOG_DEBUG, 0,
            "dn_request_check_connection, ev timeout, conn_fd: %d", c->fd);
		
        dn_request_close(r, DN_REQUEST_ERROR_TIMEOUT);
		
        return;
    }

	errno = 0;

	rs = recv(c->fd, buf, 1, MSG_PEEK);
	if (rs > 0) 
	{
        return;
    }

    if (rs == 0) 
	{
        if (ev->write) 
		{
            return;
        }
    }

    if (errno == DFS_EAGAIN || errno == DFS_EINTR) 
	{
        return;
    }

    dfs_log_debug(dfs_cycle->error_log, DFS_LOG_DEBUG, errno, 
		"client prematurely to close the connection");

    dn_request_close(r, DN_REQUEST_ERROR_CONN);
}

static int send_header_response(dn_request_t *r)
{
    conn_t             *c = NULL;
	chain_output_ctx_t *ctx = NULL;

	c = r->conn;
	ctx = r->output;

	while (c->write->ready && ctx->out) 
	{
        ctx->out = c->send_chain(c, ctx->out, 0);

		if (ctx->out == DFS_CHAIN_ERROR || !c->write->ready) 
		{ 
            break;
        }
	}

	if (ctx->out == DFS_CHAIN_ERROR) 
	{ 
        return DFS_ERROR;
    }
    
	if (ctx->out) 
	{
		return DFS_AGAIN;
	}
	
    return DFS_OK;
}

static void dn_request_process_body(dn_request_t *r)
{
    dfs_thread_t *thread = NULL;
	conn_t       *c = NULL;

	thread = get_local_thread();
	c = r->conn;
	
    if (!r->fio) 
	{
	    r->fio = cfs_fio_manager_alloc(&thread->fio_mgr);
		if (!r->fio) 
		{
            memset(&r->ev_timer, 0x00, sizeof(event_t));
            r->ev_timer.handler = fio_task_alloc_timeout;
            r->ev_timer.data = r;

            event_timer_add(c->ev_timer, &r->ev_timer, WAIT_FIO_TASK_TIMEOUT);
		
            return;
		}
    }
	
    if (r->header.op_type == OP_WRITE_BLOCK) 
	{
        dn_request_recv_block(r);
	}
	else if (r->header.op_type == OP_READ_BLOCK)
	{
        dn_request_send_block(r);
	}
}

static void dn_request_send_block(dn_request_t *r)
{
	conn_t                *c = NULL;
	sendfile_chain_task_t *sf_chain_task = NULL;

	c = r->conn;

	r->write_event_handler = dn_request_block_writing;

    if (!r->fio->sf_chain_task) 
	{
        sf_chain_task = (sendfile_chain_task_t *)pool_alloc(r->pool, 
			sizeof(sendfile_chain_task_t));

		if (!sf_chain_task) 
		{
            dfs_log_error(dfs_cycle->error_log, DFS_LOG_ALERT, 0, 
				"pool_alloc failed");

		    dn_request_close(r, DN_STATUS_INTERNAL_SERVER_ERROR);

		    return;
		}

		sf_chain_task->conn_fd = c->fd;
        sf_chain_task->store_fd = r->store_fd;
		
		r->fio->sf_chain_task = sf_chain_task;
	}

	r->fio->fd = r->store_fd;
	r->fio->offset = r->header.start_offset;
    r->fio->need = r->header.len;
    r->fio->data = r;
    r->fio->h = block_read_complete;
    r->fio->io_event = &get_local_thread()->io_events;
    r->fio->faio_ret = DFS_ERROR;
    r->fio->faio_noty = &get_local_thread()->faio_notify;
	
    if (cfs_sendfile_chain((cfs_t *)dfs_cycle->cfs, r->fio, 
		dfs_cycle->error_log) != DFS_OK) 
	{
        dn_request_close(r, DN_STATUS_INTERNAL_SERVER_ERROR);
    }
}

static void fio_task_alloc_timeout(event_t *ev)
{
    dn_request_t *r = NULL;
	conn_t       *c = NULL;

	r = (dn_request_t *)ev->data;
	c = r->conn;

	if (!ev->timedout) 
	{
        dfs_log_error(dfs_cycle->error_log, DFS_LOG_ALERT, 0, 
			"event error, not timer out, conn_fd: %d", c->fd);
		
        dn_request_close(r, DN_REQUEST_ERROR_CONN);
		
        return;
    }

	dn_request_process_body(r);
}

static int block_read_complete(void *data, void *task)
{
    dn_request_t *r = NULL;
	conn_t       *c = NULL;
	file_io_t    *fio = NULL;
	int           rs = DFS_ERROR;

	r = (dn_request_t *)data;
	c = r->conn;
	fio = (file_io_t *)task;
	rs = fio->faio_ret;

	if (rs == DFS_EAGAIN) 
	{
	    r->write_event_handler = dn_request_send_block_again;
		
	    if (c->write->ready) 
	    {
            dn_request_send_block_again(r);
		
            return DFS_OK;
        }
		
	    if (event_handle_write(c->ev_base, c->write, 0) == DFS_ERROR) 
	    {
            dfs_log_error(dfs_cycle->error_log, DFS_LOG_ALERT, 0, 
				"add write event failed");
        
            dn_request_close(r, DN_REQUEST_ERROR_CONN);
		
            return DFS_ERROR;
        }
    
        event_timer_add(c->ev_timer, c->write, CONN_TIME_OUT);
	
        return DFS_OK;
	}
	else if (rs == DFS_ERROR) 
	{
	    dfs_log_error(dfs_cycle->error_log, DFS_LOG_ALERT, 0, 
			"send block failed");
		
	    dn_request_close(r, DN_REQUEST_ERROR_CONN);
		
        return DFS_ERROR;
	}

	dn_request_read_done_response(r);

	dn_request_close(r, DN_REQUEST_ERROR_NONE);
	
    return DFS_OK;
}

static void dn_request_send_block_again(dn_request_t *r)
{
    conn_t       *c = NULL;
	event_t      *wev = NULL;
	dfs_thread_t *thread = NULL;

	c = r->conn;
	wev = c->write;
	thread = get_local_thread();

	if (wev->timedout) 
	{
	    dfs_log_debug(dfs_cycle->error_log, DFS_LOG_DEBUG, 0, 
			"wev timeout, conn_fd: %d", c->fd);
		
		dn_request_close(r, DN_REQUEST_ERROR_CONN);

		return;
    }

	if (wev->timer_set) 
	{
        event_timer_del(c->ev_timer, wev);
    }
	
    if (cfs_sendfile_chain((cfs_t *)dfs_cycle->cfs, r->fio, 
		dfs_cycle->error_log) != DFS_OK) 
	{
        dn_request_close(r, DN_STATUS_INTERNAL_SERVER_ERROR);
    }
}

static void dn_request_recv_block(dn_request_t *r)
{
    conn_t  *c = NULL;
    event_t *rev = NULL;

    c = r->conn;
	rev = c->read;

	r->read_event_handler = recv_block_handler;
    r->write_event_handler = dn_request_block_writing;

	if (rev->ready) 
	{
        recv_block_handler(r);
		
        return;
    }

	if (event_handle_read(c->ev_base, rev, 0) == DFS_ERROR) 
	{
        dfs_log_error(dfs_cycle->error_log, DFS_LOG_ALERT, 0, 
			"add read event failed");
		
        dn_request_close(r, DN_STATUS_INTERNAL_SERVER_ERROR);

		return;
    }

	event_timer_add(c->ev_timer, rev, CONN_TIME_OUT);
}

static void recv_block_handler(dn_request_t *r)
{
    int      rs = 0;
	size_t   blen = 0;
	conn_t  *c = NULL;
	event_t *rev = NULL;

	c = r->conn;
	rev = c->read;

	if (rev->timedout) 
	{
	    dfs_log_debug(dfs_cycle->error_log, DFS_LOG_DEBUG, 0, 
			"rev timeout, conn_fd: %d", c->fd);
		
		dn_request_close(r, DN_REQUEST_ERROR_CONN);

		return;
    }

	if (rev->timer_set) 
	{
        event_timer_del(c->ev_timer, rev);
    }

	while (1) 
	{
		buffer_shrink(r->input);
		
    	blen = buffer_free_size(r->input);
	    if (!blen) 
		{
	        break;
	    }
		
   		rs = c->recv(c, r->input->last, blen);
		if (rs > 0) 
		{
			r->input->last += rs;
			
			continue;
		}
		
	    if (rs == 0) 
		{
	        dfs_log_debug(dfs_cycle->error_log, DFS_LOG_DEBUG, 0, 
				"client is closed, conn_fd: %d", c->fd);
		
		    dn_request_close(r, DN_REQUEST_ERROR_CONN);

		    return;
	    }
		
	    if (rs == DFS_ERROR) 
		{
	        dfs_log_debug(dfs_cycle->error_log, DFS_LOG_DEBUG, errno, 
				"net err, conn_fd: %d", c->fd);
		
		    dn_request_close(r, DN_REQUEST_ERROR_CONN);

			return;
	    }
		
	    if (rs == DFS_AGAIN) 
		{
			break;
	    }
	}

	r->read_event_handler = dn_request_block_reading;

	r->fio->fd = r->store_fd;
	r->fio->b = r->input;
	r->fio->need = buffer_size(r->input);
	r->fio->offset = r->done;
    r->fio->data = r;
    r->fio->h = block_write_complete;
    r->fio->io_event = &get_local_thread()->io_events;
    r->fio->faio_ret = DFS_ERROR;
    r->fio->faio_noty = &get_local_thread()->faio_notify;
	
    if (cfs_write((cfs_t *)dfs_cycle->cfs, r->fio, 
		dfs_cycle->error_log) != DFS_OK) 
	{
        dn_request_close(r, DN_STATUS_INTERNAL_SERVER_ERROR);
    }
}

static int block_write_complete(void *data, void *task)
{
    dn_request_t *r = NULL;
	file_io_t    *fio = NULL;
	int           rs = DFS_ERROR;

	r = (dn_request_t *)data;
	fio = (file_io_t *)task;
	rs = fio->faio_ret;

	if (rs == DFS_ERROR) 
	{
	    dfs_log_error(dfs_cycle->error_log, DFS_LOG_ALERT, 0, 
			"do fio task failed");

		dn_request_close(r, DN_STATUS_INTERNAL_SERVER_ERROR);
		
        return DFS_ERROR;
	}

	if (rs != fio->need) 
	{
        dfs_log_error(dfs_cycle->error_log, DFS_LOG_ALERT, 0, 
			"write block failed, rs: %d, need: %d", rs, fio->need);

		dn_request_close(r, DN_STATUS_INTERNAL_SERVER_ERROR);
		
        return DFS_ERROR;
	}

	r->done += rs;
	if (r->done < r->header.len) 
	{
	    buffer_reset(r->input);
		//dn_request_recv_block(r);
		recv_block_handler(r);
		
        return DFS_OK;
	}

	cfs_close((cfs_t *)dfs_cycle->cfs, r->store_fd);
	r->store_fd = -1;

	write_block_done(r);

	dn_request_write_done_response(r);

	dn_request_close(r, DN_REQUEST_ERROR_NONE);

    return DFS_OK;
}

static void dn_request_write_done_response(dn_request_t *r)
{
    data_transfer_header_rsp_t  header_rsp;
	chain_t                    *out = NULL;
	buffer_t                   *b = NULL;
	conn_t                     *c = NULL;
    int                         header_sz = 0;

	header_rsp.op_status = OP_STATUS_SUCCESS;
	header_rsp.err = DFS_OK;
	
	c = r->conn;
	header_sz = sizeof(data_transfer_header_rsp_t);

	out = chain_alloc(r->pool);
	if (!out) 
	{
	    dfs_log_error(dfs_cycle->error_log, DFS_LOG_ALERT, 0, 
			"chain_alloc failed");

		dn_request_close(r, DN_STATUS_INTERNAL_SERVER_ERROR);

		return;
	}

	b = buffer_create(r->pool, header_sz);
	if (!b) 
	{
	    dfs_log_error(dfs_cycle->error_log, DFS_LOG_ALERT, 0, 
			"buffer_create failed");

		dn_request_close(r, DN_STATUS_INTERNAL_SERVER_ERROR);

		return;
	}

	out->buf = b;
	b->last = memory_cpymem(b->last, &header_rsp, header_sz);

    if (!r->output) 
	{
        r->output = (chain_output_ctx_t *)pool_alloc(r->pool, 
			sizeof(chain_output_ctx_t));
		if (!r->output) 
		{
            dfs_log_error(dfs_cycle->error_log, DFS_LOG_ALERT, 0, 
				"pool_alloc failed");

		    dn_request_close(r, DN_STATUS_INTERNAL_SERVER_ERROR);

		    return;
		}
	}

	r->output->out = NULL;
	chain_append_all(&r->output->out, out);

	r->write_event_handler = dn_request_send_write_done_response;
    r->read_event_handler = dn_request_check_read_connection;

	if (c->write->ready) 
	{
        dn_request_send_write_done_response(r);
		
        return;
    }

	if (event_handle_write(c->ev_base, c->write, 0) == DFS_ERROR) 
	{
        dfs_log_error(dfs_cycle->error_log, DFS_LOG_ALERT, 0, 
			"add write event failed");
        
        dn_request_close(r, DN_REQUEST_ERROR_SPECIAL_RESPONSE);
		
        return;
    }
    
    event_timer_add(c->ev_timer, c->write, CONN_TIME_OUT);
}

static void dn_request_send_write_done_response(dn_request_t *r)
{
    conn_t  *c = NULL;
	event_t *wev = NULL;
	int      rs = 0;

	c = r->conn;
	wev = c->write;

	if (wev->timedout) 
	{
	    dfs_log_debug(dfs_cycle->error_log, DFS_LOG_DEBUG, 0, 
			"dn_request_send_header_response, wev timeout, conn_fd: %d", c->fd);
		
		dn_request_close(r, DN_REQUEST_ERROR_CONN);

		return;
    }

	if (wev->timer_set) 
	{
        event_timer_del(c->ev_timer, wev);
    }

	rs = send_header_response(r);
	if (rs == DFS_OK) 
	{
	    return;
	}
	else if (rs == DFS_AGAIN) 
	{
        event_timer_add(c->ev_timer, wev, CONN_TIME_OUT);
		
        return;
    }

	dn_request_close(r, DN_REQUEST_ERROR_SPECIAL_RESPONSE);
}

static void dn_request_read_done_response(dn_request_t *r)
{
    data_transfer_header_rsp_t  header_rsp;
	chain_t                    *out = NULL;
	buffer_t                   *b = NULL;
	conn_t                     *c = NULL;
    int                         header_sz = 0;

	header_rsp.op_status = OP_STATUS_SUCCESS;
	header_rsp.err = DFS_OK;
	
	c = r->conn;
	header_sz = sizeof(data_transfer_header_rsp_t);

	out = chain_alloc(r->pool);
	if (!out) 
	{
	    dfs_log_error(dfs_cycle->error_log, DFS_LOG_ALERT, 0, 
			"chain_alloc failed");

		dn_request_close(r, DN_STATUS_INTERNAL_SERVER_ERROR);

		return;
	}

	b = buffer_create(r->pool, header_sz);
	if (!b) 
	{
	    dfs_log_error(dfs_cycle->error_log, DFS_LOG_ALERT, 0, 
			"buffer_create failed");

		dn_request_close(r, DN_STATUS_INTERNAL_SERVER_ERROR);

		return;
	}

	out->buf = b;
	b->last = memory_cpymem(b->last, &header_rsp, header_sz);

    if (!r->output) 
	{
        r->output = (chain_output_ctx_t *)pool_alloc(r->pool, 
			sizeof(chain_output_ctx_t));
		if (!r->output) 
		{
            dfs_log_error(dfs_cycle->error_log, DFS_LOG_ALERT, 0, 
				"pool_alloc failed");

		    dn_request_close(r, DN_STATUS_INTERNAL_SERVER_ERROR);

		    return;
		}
	}

	r->output->out = NULL;
	chain_append_all(&r->output->out, out);

	r->write_event_handler = dn_request_send_read_done_response;
    r->read_event_handler = dn_request_check_read_connection;

	if (c->write->ready) 
	{
        dn_request_send_read_done_response(r);
		
        return;
    }

	if (event_handle_write(c->ev_base, c->write, 0) == DFS_ERROR) 
	{
        dfs_log_error(dfs_cycle->error_log, DFS_LOG_ALERT, 0, 
			"add write event failed");
        
        dn_request_close(r, DN_REQUEST_ERROR_SPECIAL_RESPONSE);
		
        return;
    }
    
    event_timer_add(c->ev_timer, c->write, CONN_TIME_OUT);
}

static void dn_request_send_read_done_response(dn_request_t *r)
{
    conn_t  *c = NULL;
	event_t *wev = NULL;
	int      rs = 0;

	c = r->conn;
	wev = c->write;

	if (wev->timedout) 
	{
	    dfs_log_debug(dfs_cycle->error_log, DFS_LOG_DEBUG, 0, 
			"dn_request_send_header_response, wev timeout, conn_fd: %d", c->fd);
		
		dn_request_close(r, DN_REQUEST_ERROR_CONN);

		return;
    }

	if (wev->timer_set) 
	{
        event_timer_del(c->ev_timer, wev);
    }

	rs = send_header_response(r);
	if (rs == DFS_OK) 
	{
	    return;
	}
	else if (rs == DFS_AGAIN) 
	{
        event_timer_add(c->ev_timer, wev, CONN_TIME_OUT);
		
        return;
    }

	dn_request_close(r, DN_REQUEST_ERROR_SPECIAL_RESPONSE);
}

