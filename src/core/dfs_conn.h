#ifndef DFS_CONN_H
#define DFS_CONN_H

#include "dfs_types.h"
#include "dfs_sysio.h"
#include "dfs_string.h"
#include "dfs_event.h"
#include "dfs_error_log.h"

//#define CONN_DEFAULT_RCVBUF    -1
//#define CONN_DEFAULT_SNDBUF    -1
#define CONN_DEFAULT_RCVBUF    (64<<10)
#define CONN_DEFAULT_SNDBUF    (64<<10)
#define CONN_DEFAULT_POOL_SIZE 2048
#define CONN_DEFAULT_BACKLOG   2048

typedef struct conn_peer_s conn_peer_t;

enum 
{
    CONN_TCP_NODELAY_UNSET = 0,
    CONN_TCP_NODELAY_SET,
    CONN_TCP_NODELAY_DISABLED
};

enum 
{
    CONN_TCP_NOPUSH_UNSET = 0,
    CONN_TCP_NOPUSH_SET,
    CONN_TCP_NOPUSH_DISABLED
};

enum 
{
    CONN_ERROR_NONE = 0,
    CONN_ERROR_REQUEST,
    CONN_ERROR_EOF
};

struct conn_s 
{
    int                    fd;
    void                  *next;
    void                  *conn_data;
    event_t               *read;
    event_t               *write;
    sysio_recv_pt          recv;
    sysio_send_pt          send;
    sysio_recv_chain_pt    recv_chain;
    sysio_send_chain_pt    send_chain;
    sysio_sendfile_pt      sendfile_chain;
    listening_t           *listening;
    size_t                 sent;
    pool_t                *pool;
    struct sockaddr       *sockaddr;
    socklen_t              socklen;
    string_t               addr_text;
    struct timeval         accept_time;
    uint32_t               error:1;
    uint32_t               sendfile:1;
    uint32_t               sndlowat:1;
    uint32_t               tcp_nodelay:2;
    uint32_t               tcp_nopush:2;
    event_timer_t         *ev_timer;
    event_base_t          *ev_base;  
    log_t                 *log;
};

struct conn_peer_s 
{
    conn_t                *connection;
    struct sockaddr       *sockaddr;
    socklen_t              socklen;
    string_t              *name;
    int                    rcvbuf;
};

#define DFS_EPERM         EPERM
#define DFS_ENOENT        ENOENT
#define DFS_ESRCH         ESRCH
#define DFS_EINTR         EINTR
#define DFS_ECHILD        ECHILD
#define DFS_ENOMEM        ENOMEM
#define DFS_EACCES        EACCES
#define DFS_EBUSY         EBUSY
#define DFS_EEXIST        EEXIST
#define DFS_ENOTDIR       ENOTDIR
#define DFS_EISDIR        EISDIR
#define DFS_EINVAL        EINVAL
#define DFS_ENOSPC        ENOSPC
#define DFS_EPIPE         EPIPE
#define DFS_EAGAIN        EAGAIN
#define DFS_EINPROGRESS   EINPROGRESS
#define DFS_EADDRINUSE    EADDRINUSE
#define DFS_ECONNABORTED  ECONNABORTED
#define DFS_ECONNRESET    ECONNRESET
#define DFS_ENOTCONN      ENOTCONN
#define DFS_ETIMEDOUT     ETIMEDOUT
#define DFS_ECONNREFUSED  ECONNREFUSED
#define DFS_ENAMETOOLONG  ENAMETOOLONG
#define DFS_ENETDOWN      ENETDOWN
#define DFS_ENETUNREACH   ENETUNREACH
#define DFS_EHOSTDOWN     EHOSTDOWN
#define DFS_EHOSTUNREACH  EHOSTUNREACH
#define DFS_ENOSYS        ENOSYS
#define DFS_ECANCELED     ECANCELED
#define DFS_ENOMOREFILES  0

#define DFS_SOCKLEN       512

#define conn_nonblocking(s)  fcntl(s, F_SETFL, fcntl(s, F_GETFL) | O_NONBLOCK)
#define conn_blocking(s)     fcntl(s, F_SETFL, fcntl(s, F_GETFL) & ~O_NONBLOCK)

int  conn_connect_peer(conn_peer_t *pc, event_base_t *ep_base);
conn_t *conn_get_from_mem(int s);
void conn_free_mem(conn_t *c);
void conn_set_default(conn_t *c, int s);
void conn_close(conn_t *c);
void conn_release(conn_t *c);
int  conn_tcp_nopush(int s);
int  conn_tcp_push(int s);
int  conn_tcp_nodelay(int s);
int  conn_tcp_delay(int s);

#endif

