#include "dfs_conn_pool.h"
#include "dfs_memory.h"
#include "dfs_event_timer.h"
#include "dfs_lock.h"
#include "dfs_conn.h"

conn_pool_t       comm_conn_pool;
dfs_atomic_lock_t comm_conn_lock;

static void put_comm_conn(conn_t *c);
static conn_t* get_comm_conn(uint32_t n, int *num);

int conn_pool_init(conn_pool_t *pool, uint32_t connection_n)
{
    conn_t   *conn = NULL;
    event_t  *revs = NULL;
    event_t  *wevs = NULL;
    uint32_t  i = 0;

    if (connection_n == 0) 
	{
        return DFS_ERROR;
    }

    pool->connection_n = connection_n;
    pool->connections = (conn_t *)memory_calloc(sizeof(conn_t) * pool->connection_n);
    if (!pool->connections) 
	{
        return DFS_ERROR;
    }

    pool->read_events = (event_t *)memory_calloc(sizeof(event_t) * pool->connection_n);
    if (!pool->read_events) 
	{
        conn_pool_free(pool);
		
        return DFS_ERROR;
    }

    pool->write_events = (event_t *)memory_calloc(sizeof(event_t) * pool->connection_n);
    if (!pool->write_events) 
	{
        conn_pool_free(pool);
		
        return DFS_ERROR;
    }

    conn = pool->connections;
    revs = pool->read_events;
    wevs = pool->write_events;

    for (i = 0; i < pool->connection_n; i++) 
	{
        revs[i].instance = 1;

        if (i == pool->connection_n - 1) 
		{
            conn[i].next = NULL;
        } 
		else 
		{
            conn[i].next = &conn[i + 1];
        }

        conn[i].fd = DFS_INVALID_FILE;
        conn[i].read = &revs[i];
        conn[i].read->timer_event = DFS_FALSE;
        conn[i].write = &wevs[i];
        conn[i].write->timer_event = DFS_FALSE;
    }

    pool->free_connections = conn;
    pool->free_connection_n = pool->connection_n;

    return DFS_OK;
}

void conn_pool_free(conn_pool_t *pool)
{
    if (!pool) 
	{
        return;
    }

    if (pool->connections) 
	{
        memory_free(pool->connections,
            sizeof(conn_t) * pool->connection_n);
        pool->connections = NULL;
    }

    if (pool->read_events) 
	{
        memory_free(pool->read_events, sizeof(event_t) * pool->connection_n);
        pool->read_events = NULL;
    }

    if (pool->write_events) 
	{
        memory_free(pool->write_events, sizeof(event_t) * pool->connection_n);
        pool->write_events = NULL;
    }

    pool->connection_n = 0;
    pool->free_connection_n = 0;
}

conn_t * conn_pool_get_connection(conn_pool_t *pool)
{
    conn_t   *c    = NULL;
    int       num  = 0;
	uint32_t  n    = 0;

    c = pool->free_connections;
    if (!c) 
	{
        if (pool->change_n >= 0) 
		{
            return NULL;
        }
		
		n = -pool->change_n;
        c = get_comm_conn(n, &num);
        if (c) 
		{
            pool->free_connections = c;
            pool->free_connection_n += num;
			pool->change_n += num;
			
            goto out_conn;
        }

        return NULL;
    }

out_conn:
    pool->free_connections = (conn_t *)c->next;
    pool->free_connection_n--;
	pool->used_n++;
	
    return c;
}

void conn_pool_free_connection(conn_pool_t *pool, conn_t *c)
{   
    if (pool->change_n > 0) 
	{
        pool->used_n--;
        put_comm_conn(c);
		pool->change_n--;
		
        return;
    }
	
    c->next = pool->free_connections;

    pool->free_connections = c;
    pool->free_connection_n++;
	pool->used_n--;

}

int conn_pool_common_init()
{
    comm_conn_lock.lock = DFS_LOCK_OFF;
    comm_conn_lock.allocator = NULL;
    memset(&comm_conn_pool, 0, sizeof(conn_pool_t));
	
    return DFS_OK;
}
int conn_pool_common_release()
{
    comm_conn_lock.lock = DFS_LOCK_OFF;
    comm_conn_lock.allocator = NULL;
	
    return DFS_OK;
}

static void put_comm_conn(conn_t *c)
{
	dfs_lock_errno_t error;
   
    dfs_atomic_lock_on(&comm_conn_lock, &error);
    comm_conn_pool.free_connection_n++;
    c->next = comm_conn_pool.free_connections;
    comm_conn_pool.free_connections = c;
    
    dfs_atomic_lock_off(&comm_conn_lock, &error);
}

static conn_t* get_comm_conn(uint32_t n, int *num)
{
	dfs_lock_errno_t  error;
    conn_t           *c = NULL, *p = NULL, *plast = NULL;
    uint32_t          i = 0;
		
	dfs_atomic_lock_on(&comm_conn_lock, &error);
	
    if (!comm_conn_pool.free_connection_n) 
	{
        dfs_atomic_lock_off(&comm_conn_lock, &error);
		
        return NULL;
    }

    if (n >= comm_conn_pool.free_connection_n) 
	{
        c = comm_conn_pool.free_connections;
        *num = comm_conn_pool.free_connection_n;

        comm_conn_pool.free_connections = NULL;
        comm_conn_pool.free_connection_n = 0;

        dfs_atomic_lock_off(&comm_conn_lock, &error);
		
        return c;
    }
	
	p = comm_conn_pool.free_connections;
    i = n;
	
	for (plast = p; p && i > 0; i--) 
	{
		plast = p;
		p = (conn_t *)p->next;
	}
	
	c = comm_conn_pool.free_connections;
    
	comm_conn_pool.free_connections = p;
    comm_conn_pool.free_connection_n -= n;
    
	plast->next = NULL;
    *num = n;
    
    dfs_atomic_lock_off(&comm_conn_lock, &error);

    return c;
}

void conn_pool_out(conn_pool_t *pool, int n) 
{
	pool->change_n -= n; 
	pool->used_n -=n; 
}
		
void conn_pool_in(conn_pool_t *pool, int n) 
{
	pool->change_n += n; 
	pool->used_n += n; 
}

