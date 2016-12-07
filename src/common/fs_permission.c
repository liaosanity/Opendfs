#include "fs_permission.h"

char *FsAction[] =
{
    "---", // NONE
    "--x", // EXECUTE
    "-w-", // WRITE
    "-wx", // WRITE_EXECUTE
    "r--", // READ
    "r-x", // READ_EXECUTE
    "rw-", // READ_WRITE
    "rwx"  // ALL
};

int check_permission(task_t *task, fi_inode_t *finode, 
	short access, uchar_t *err)
{
    short u = 0, g = 0, o = 0, c = 0;

	o = finode->permission % 10;
    c = finode->permission / 10;
    g = c % 10;
    u = c / 10;

	if (0 == string_strncmp(task->user, finode->owner, 
		string_strlen(finode->owner)))
	{
	    if (u == access)
	    {
	        return DFS_OK;
	    }
	} 
	else if (0 == string_strncmp(task->group, finode->group, 
		string_strlen(finode->group)))
	{
	    if (g == access)
	    {
	        return DFS_OK;
	    }
	}
	else
	{
	    if (o == access)
	    {
	        return DFS_OK;
	    }
	}

	string_xxsprintf(err, "Permission denied: user=%s, access=%s", 
		task->user, FsAction[access]);
	
    return DFS_ERROR;
}

int is_super(char user[], string_t *admin)
{
    return (0 == string_strncmp(user, admin->data, admin->len)) 
		? DFS_TRUE : DFS_FALSE;
}

void get_permission(short permission, uchar_t *str)
{
    short u = 0, g = 0, o = 0, c = 0;

	o = permission % 10;
    c = permission / 10;
    g = c % 10;
    u = c / 10;

	string_xxsprintf(str, "%s%s%s", FsAction[u], FsAction[g], FsAction[o]);
}

