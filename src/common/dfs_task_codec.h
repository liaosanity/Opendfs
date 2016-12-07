#ifndef DFS_TASK_CODEC_H
#define DFS_TASK_CODEC_H

#include "dfs_buffer.h"
#include "dfs_types.h"
#include "dfs_task.h"

int task_decode(buffer_t *buff, task_t *task);
int task_encode(task_t *task, buffer_t *buff);

#endif

