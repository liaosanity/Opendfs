#include "dfs_chain.h"
#include "dfs_conn.h"
#include "dfs_memory.h"

chain_t * chain_alloc(pool_t *pool)
{
    chain_t *cl = NULL;

    if (!pool) 
	{
        return NULL;
    }
	
    cl = (chain_t *)pool_alloc(pool, sizeof(chain_t));
    if (!cl) 
	{
        return NULL;
    }
	
    cl->next = NULL;

    return cl;
}

int chain_reset(chain_t *cl)
{
    for (; cl; cl = cl->next) 
	{
        cl->buf->pos = cl->buf->last = cl->buf->start;
    }
  
    return DFS_OK;
}

int chain_empty(chain_t *cl)
{
    for (; cl; cl = cl->next) 
	{
        if (buffer_size(cl->buf) > 0) 
		{
            return DFS_FALSE;
        }
    }
  
    return DFS_TRUE;
}

uint64_t chain_size(chain_t *in) 
{
    uint64_t len = 0;

    while (in) 
	{
        len += buffer_size(in->buf);
        in = in->next;
    }

    return len;
}

int chain_output(chain_output_ctx_t *ctx, chain_t *in)
{
    conn_t *c = NULL;
    
    if (!ctx) 
	{
        return DFS_ERROR;
    }
	
    if (in) 
	{
        chain_append_all(&ctx->out, in);
    }
	
    if (chain_empty(ctx->out)) 
	{
        return DFS_OK;
    }
	
    c = ctx->connection;
    if (!c) 
	{
        return DFS_ERROR;
    }

	while (c->write->ready && ctx->out) 
	{
	    if (ctx->out->buf->memory) 
		{
			//sysio_writev_chain
	        ctx->out = c->send_chain(c, ctx->out, ctx->limit);
	    } 
		else 
		{
	        //sysio_sendfile_chain
	        ctx->out = c->sendfile_chain(c, ctx->out, ctx->fd, ctx->limit);
	    }
		
	    if (ctx->out == DFS_CHAIN_ERROR) 
		{
        
	        return DFS_ERROR;
	    }
	}

	if (ctx->out) 
	{
		return DFS_AGAIN;
	}
	
    return DFS_OK;
}

int chain_output_with_limit(chain_output_ctx_t *ctx, chain_t *in, 
	                                size_t limit)
{
    conn_t *c = NULL;
    size_t sent = 0;
    int64_t cur_limit = limit;
    
    c = ctx->connection;
    sent = c->sent;
	
    while (c->write->ready && ctx->out) 
	{
        if (ctx->out->buf->memory) 
		{
            //sysio_writev_chain
            ctx->out = c->send_chain(c, ctx->out, cur_limit);
        } 
		else 
		{
            //sysio_sendfile_chain
            ctx->out = c->sendfile_chain(c, ctx->out, ctx->fd, cur_limit);
        }
        
        if (ctx->out == DFS_CHAIN_ERROR || !c->write->ready) 
		{
            break;
        }
        
        if (limit) 
		{
            cur_limit -= c->sent - sent;
            sent = c->sent;
			
            if (cur_limit <= 0) 
			{
                break;
            }
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

void chain_append_withsize(chain_t **dst_chain, chain_t *src_chain, 
	                                size_t size, chain_t **free_chain)
{
    size_t buf_size = 0;
    
    if (!dst_chain || !src_chain || size == 0) 
	{
        return;
    }
	
    while (*dst_chain) 
	{
        dst_chain = &((*dst_chain)->next);
    }
	
    while (src_chain && size > 0) 
	{
        buf_size = buffer_size(src_chain->buf);
        *dst_chain = src_chain;
        src_chain = src_chain->next;
        (*dst_chain)->next = NULL;
        dst_chain = &((*dst_chain)->next);
        size -= buf_size;
    }
	
    // put other chain into free_chain
    if (src_chain) 
	{
        *free_chain = src_chain;
    }
}

void chain_append_all(chain_t **dst_chain, chain_t *src_chain)
{
    if (!dst_chain || !src_chain) 
	{
        return;
    }
	
    while (*dst_chain) 
	{
        dst_chain = &((*dst_chain)->next);
    }
	
    *dst_chain = src_chain;
}

int chain_append_buffer(pool_t *pool, chain_t **dst_chain,
                                buffer_t *src_buffer)
{
    chain_t *ln = NULL;
    
    if (!pool || !dst_chain || !src_buffer) 
	{
        return DFS_OK;
    }
	
    while (*dst_chain) 
	{
        dst_chain = &((*dst_chain)->next);
    }
	
    ln = chain_alloc(pool);
    if (!ln) 
	{
        return DFS_ERROR;
    }
	
    ln->buf = src_buffer;
    *dst_chain = ln;

    return DFS_OK;
}

int chain_append_buffer_withsize(pool_t *pool, chain_t **dst_chain, 
	                                        buffer_t *src_buffer, size_t size)
{
    chain_t *ln = NULL;
    
    if (!pool || !dst_chain || !src_buffer) 
	{
        return DFS_OK;
    }
	
    while (*dst_chain) 
	{
        dst_chain = &((*dst_chain)->next);
    }
	
    ln = chain_alloc(pool);
    if (!ln) 
	{
        return DFS_ERROR;
    }
	
    ln->buf = src_buffer;
    *dst_chain = ln;

    return DFS_OK;
}

void chain_read_update(chain_t *chain, size_t size)
{
    size_t chain_size = 0;
    
    if (!chain) 
	{
        return;
    }
	
    while (chain && size > 0) 
	{
        chain_size = chain->buf->end - chain->buf->last;
		
        if (size >= chain_size) 
		{
            chain->buf->last = chain->buf->end;
            size -= chain_size;
        } 
		else 
		{
            chain->buf->last += size;
            size = 0;
        }
		
        chain = chain->next;
    }
}

chain_t * chain_write_update(chain_t *chain, size_t size)
{
    size_t bsize = 0;
    
    while (chain && size > 0) 
	{
        bsize = buffer_size(chain->buf);
        if (size < bsize) 
		{
			if (chain->buf->memory == DFS_TRUE) 
			{
	            chain->buf->pos += size;
			} 
			else 
			{
				chain->buf->file_pos += size;
			}
			
            return chain;
        }
		
        size -= bsize;
		
		if (chain->buf->memory == DFS_TRUE) 
		{
            chain->buf->pos = chain->buf->last;
		} 
		else 
		{
			chain->buf->file_pos = chain->buf->file_last;
		}
		
        chain = chain->next;
    }

    return chain;
}

