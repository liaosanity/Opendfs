#ifndef DFS_NOTICE_H
#define DFS_NOTICE_H

#include "dfs_types.h"
#include "dfs_event.h"
#include "dfs_pipe.h"
#include "dfs_epoll.h"

typedef struct notice_s notice_t;
typedef int  (*wake_up_ptr)(notice_t *n);
typedef void (*wake_up_hander)(void *data);

struct notice_s 
{
    pipe_t          channel;
    wake_up_ptr     wake_up;
    wake_up_hander  call_back;
    void           *data;
    log_t          *log;
};

int notice_init(event_base_t *base, notice_t *n, wake_up_hander handler, 
	void *data);
int notice_wake_up(notice_t *n);

#endif

