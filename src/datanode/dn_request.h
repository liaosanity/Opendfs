#ifndef DN_REQUEST_H
#define DN_REQUEST_H

#include "dfs_types.h"
#include "dfs_conn.h"
#include "dfs_chain.h"
#include "cfs_fio.h"
#include "dfs_task_cmd.h"

#define CONN_POOL_SZ  4096
#define CONN_TIME_OUT 60000

#define WAIT_FIO_TASK_TIMEOUT 500

#define DN_STATUS_CLIENT_CLOSED_REQUEST         499
#define DN_STATUS_INTERNAL_SERVER_ERROR         500
#define DN_STATUS_NOT_IMPLEMENTED               501
#define DN_STATUS_BAD_GATEWAY                   502
#define DN_STATUS_SERVICE_UNAVAILABLE           503
#define DN_STATUS_GATEWAY_TIME_OUT              504
#define DN_STATUS_VERSION_NOT_SUPPORTED         505
#define DN_STATUS_INSUFFICIENT_STORAGE          507
#define DN_STATUS_CC_DENEY                      555

typedef enum dn_request_error_s 
{
    DN_REQUEST_ERROR_NONE = 0, 
    DN_REQUEST_ERROR_ZERO_OUT,
    DN_REQUEST_ERROR_SPECIAL_RESPONSE,
    DN_REQUEST_ERROR_TIMEOUT,
    DN_REQUEST_ERROR_READ_REQUEST,
    DN_REQUEST_ERROR_CONN,
    DN_REQUEST_ERROR_BLK_NO_EXIST,
    DN_REQUEST_ERROR_DISK_FAILED,
    DN_REQUEST_ERROR_IO_FAILED,
    DN_REQUEST_ERROR_PSEUDO,
    DN_REQUEST_ERROR_COMBIN,
    DN_REQUEST_ERROR_STREAMING,
    DN_REQUEST_ERROR_STOP_PLAY,
} dn_request_error_t;

typedef struct dn_request_s dn_request_t;
typedef void (*dn_event_handler_pt)(dn_request_t *);

typedef struct dn_request_s 
{
    conn_t                 *conn;
    dn_event_handler_pt     read_event_handler;
    dn_event_handler_pt     write_event_handler;
    pool_t                 *pool;
	buffer_t               *input;
	chain_output_ctx_t     *output;
	event_t			        ev_timer;
    char                    ipaddr[32];
	data_transfer_header_t  header;
	int                     store_fd;
	uchar_t                *path;
	long                    done;
	file_io_t              *fio;
} dn_request_t;

void dn_conn_init(conn_t *c);
void dn_request_init(event_t *rev);

#endif

