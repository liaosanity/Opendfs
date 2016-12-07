#ifndef DFS_CONN_POOL_H
#define DFS_CONN_POOL_H

#include "dfs_types.h"

typedef struct conn_pool_s conn_pool_t;

struct conn_pool_s 
{
    conn_t   *connections;
    uint32_t  connection_n;      //all connections
    int       change_n;          
    conn_t   *free_connections;
    uint32_t  free_connection_n; //free connections that can be used
    event_t  *read_events;
    event_t  *write_events;
	uint32_t  used_n;
} ;

int conn_pool_common_init();
int conn_pool_common_release();
int  conn_pool_init(conn_pool_t *conn_pool, uint32_t connection_n);
void conn_pool_free(conn_pool_t *conn_pool);
conn_t *conn_pool_get_connection(conn_pool_t *pool);
void conn_pool_free_connection(conn_pool_t *pool, conn_t *c);
void conn_pool_out(conn_pool_t *pool, int n) ;	
void conn_pool_in(conn_pool_t *pool, int n); 

#endif

