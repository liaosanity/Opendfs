#ifndef DFS_EPOLL_H
#define DFS_EPOLL_H

#include "dfs_types.h"
#include "dfs_queue.h"
#include "dfs_error_log.h"

typedef struct epoll_event epoll_event_t;

typedef void (*time_update_ptr)();

struct event_base_s 
{
    int             ep;
    uint32_t        event_flags;
    epoll_event_t  *event_list;
    uint32_t        nevents;
    time_update_ptr time_update;
    queue_t         posted_accept_events;
    queue_t         posted_events;
    log_t           *log;
};

int  epoll_init(event_base_t *ep_base, log_t *log);
void epoll_done(event_base_t *ep_base);
int  epoll_add_event(event_base_t *ep_base, event_t *ev, int event, uint32_t flags);
int  epoll_del_event(event_base_t *ep_base,event_t *ev, int event, uint32_t flags);
int  epoll_add_connection(event_base_t *ep_base, conn_t *c);
int  epoll_del_connection(event_base_t *ep_base, conn_t *c, uint32_t flags);
int  epoll_process_events(event_base_t *ep_base,
    rb_msec_t timer, uint32_t flags);

#endif

