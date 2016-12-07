#ifndef DFS_CHANNEL_H
#define DFS_CHANNEL_H

#include "dfs_types.h"
#include "dfs_event.h"

typedef struct channel_s 
{
     uint32_t command;
     pid_t    pid;
     int      slot;
     int      fd;
} channel_t;

enum 
{
    CHANNEL_CMD_NONE = 0,
    CHANNEL_CMD_OPEN,
    CHANNEL_CMD_CLOSE,
    CHANNEL_CMD_QUIT,
    CHANNEL_CMD_TERMINATE,
    CHANNEL_CMD_REOPEN,
    CHANNEL_CMD_BACKUP,
};

int  channel_write(int socket, channel_t *ch,
    size_t size, log_t *log);
int  channel_read(int socket, channel_t *ch,
    size_t size, log_t *log);

void channel_close(int *fd, log_t *log);

#endif

