#ifndef FS_PERMISSION_H
#define FS_PERMISSION_H

#include "dfs_string.h"
#include "nn_file_index.h"

#define NONE          0
#define EXECUTE       1
#define WRITE         2
#define WRITE_EXECUTE 3
#define READ          4
#define READ_EXECUTE  5
#define READ_WRITE    6
#define ALL           7

int check_permission(task_t *task, fi_inode_t *finode, 
	short access, uchar_t *err);
int is_super(char user[], string_t *admin);
void get_permission(short permission, uchar_t *str);

#endif

