#ifndef DFS_TASK_H_
#define DFS_TASK_H_

#include <stdint.h>
#include <stddef.h>

#include "dfs_task_cmd.h"

#define TASK_ERROR -1
#define TASK_EAGIN -2
#define TASK_OK 0

#define KEY_LEN   256
#define OWNER_LEN 16
#define GROUP_LEN 16

typedef struct task_s
{
	cmd_t     cmd;
	int       ret;
	uint32_t  seq;
	void     *opq;
	int       master_nodeid;
	char      key[KEY_LEN];
	char      user[OWNER_LEN];
	char      group[GROUP_LEN];
	short     permission;
	int       data_len; 
	void     *data;
} task_t;

task_t * task_new();
void task_free(task_t* task);
void task_clear(task_t* task);
int task_encode2str(task_t *task, char *buff, int len);
int task_decodefstr(char *buff, int len, task_t *task);

#endif

