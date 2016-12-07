#include "nn_rpc_server.h"
#include "nn_thread.h"
#include "nn_net_response_handler.h"
#include "dfs_task.h"
#include "nn_task_handler.h"
#include "nn_conf.h"
#include "nn_file_index.h"
#include "nn_dn_index.h"

int nn_rpc_worker_init(cycle_t *cycle)
{
    //conf_server_t *conf = NULL;
	//conf = (conf_server_t *)cycle->sconf;

    return DFS_OK;
}

int nn_rpc_worker_release(cycle_t *cycle)
{
    (void) cycle;

    return DFS_OK;
}

int nn_rpc_service_run(task_t *task)
{
    int optype = task->cmd;
	
	switch (optype)
    {
    case NN_MKDIR:
		nn_mkdir(task);
		break;

	case NN_RMR:
		nn_rmr(task);
		break;

	case NN_LS:
		nn_ls(task);
		break;
		
	case NN_GET_FILE_INFO:
		nn_get_file_info(task);
		break;

	case NN_CREATE:
		nn_create(task);
		break;

	case NN_GET_ADDITIONAL_BLK:
		nn_get_additional_blk(task);
		break;

	case NN_CLOSE:
		nn_close(task);
		break;

	case NN_RM:
		nn_rm(task);
		break;

	case NN_OPEN:
		nn_open(task);
		break;

	case DN_REGISTER:
		nn_dn_register(task);
		break;

	case DN_HEARTBEAT:
		nn_dn_heartbeat(task);
		break;

	case DN_RECV_BLK_REPORT:
		nn_dn_recv_blk_report(task);
		break;

	case DN_DEL_BLK_REPORT:
		nn_dn_del_blk_report(task);
		break;

	case DN_BLK_REPORT:
		nn_dn_blk_report(task);
		break;
		
	default:
		dfs_log_error(dfs_cycle->error_log, DFS_LOG_ALERT, 0, 
			"unknown optype: ", optype);
		
		return DFS_ERROR;
	}
	
    return DFS_OK;
}

