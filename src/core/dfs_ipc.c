#include "dfs_ipc.h"
 
int dfs_ipc_open(string_t *write_path)
{
    int      to_log_fds[2];
    pid_t    ipc_pid = 0;
    uint32_t i = 0;

    if (!write_path) 
	{
        return DFS_ERROR;
    }
	
    if (pipe(to_log_fds)) 
	{
        return DFS_ERROR;
    }

    switch (ipc_pid = fork()) 
	{
    case 0:
        // child process
        close(STDIN_FILENO);
        close(STDOUT_FILENO);
        dup2(to_log_fds[0], STDIN_FILENO);
        close(to_log_fds[0]);
        close(to_log_fds[1]);
        // we don't need the client socket
        for (i = 3; i < 256; i++) 
		{
            close(i);
        }
		
        // exec the log-process (skip the | )
        execl("/bin/sh", "sh", "-c", write_path->data + 1, NULL);
        exit(-1);
        break;
		
    case -1:
        // error
        printf("ipc open failed %s\n", write_path->data);
        break;
		
    default:
        close(to_log_fds[0]);
        // return writed side
        return to_log_fds[1];
    }
    
    return DFS_ERROR;
}

