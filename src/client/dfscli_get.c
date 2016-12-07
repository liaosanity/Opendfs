#include "dfscli_get.h"
#include "dfs_memory_pool.h"
#include "dfs_task_cmd.h"
#include "dfs_task.h"
#include "dfscli_conf.h"
#include "dfscli_cycle.h"

static int dfs_open(rw_context_t *rw_ctx);
static int dfs_read_blk(rw_context_t *rw_ctx);
static int do_recvfile_splice(int fromfd, int tofd, 
	loff_t *offset, size_t count);

int dfscli_get(char *src, char *dst)
{
    conf_server_t *sconf = NULL;
    rw_context_t  *rw_ctx = NULL;

	sconf = (conf_server_t *)dfs_cycle->sconf;
	
	rw_ctx = (rw_context_t *)pool_alloc(dfs_cycle->pool, sizeof(rw_context_t));
	if (!rw_ctx) 
	{
	    dfscli_log(DFS_LOG_WARN, "dfscli_get, pool_alloc err");
		
        return DFS_ERROR;
	}

	strcpy(rw_ctx->src, src);
	strcpy(rw_ctx->dst, dst);
	rw_ctx->blk_sz = sconf->blk_sz;
	rw_ctx->blk_rep = sconf->blk_rep;
	
	if (dfs_open(rw_ctx) != DFS_OK) 
	{
        return DFS_ERROR;
	}

	dfs_read_blk(rw_ctx);

	//dfs_close(rw_ctx);

	if (rw_ctx->nn_fd > 0) 
	{
        close(rw_ctx->nn_fd);
		rw_ctx->nn_fd = -1;
	}

	for (int i = 0; i < rw_ctx->dn_num; i++) 
	{
        if (rw_ctx->dn_fd[i] > 0) 
		{
		    close(rw_ctx->dn_fd[i]);
            rw_ctx->dn_fd[i] = -1;
		}
	}
	
    return DFS_OK;
}

static int dfs_open(rw_context_t *rw_ctx)
{
    conf_server_t     *sconf = NULL;
    server_bind_t     *nn_addr = NULL;
	create_blk_info_t  blk_info;
	create_resp_info_t resp_info;

	sconf = (conf_server_t *)dfs_cycle->sconf;
	nn_addr = (server_bind_t *)sconf->namenode_addr.elts;
	
    int sockfd = dfs_connect((char *)nn_addr[0].addr.data, nn_addr[0].port);
	if (sockfd < 0) 
	{
	    return DFS_ERROR;
	}
	
	task_t out_t;
	bzero(&out_t, sizeof(task_t));
	out_t.cmd = NN_OPEN;
	keyEncode((uchar_t *)rw_ctx->src, (uchar_t *)out_t.key);

	getUserInfo(&out_t);

	out_t.permission = 755;

	memset(&blk_info, 0x00, sizeof(create_blk_info_t));
	blk_info.blk_sz = rw_ctx->blk_sz;
	blk_info.blk_rep = rw_ctx->blk_rep;

	out_t.data_len = sizeof(create_blk_info_t);
	out_t.data = &blk_info;

	char sBuf[BUF_SZ] = "";
	int sLen = task_encode2str(&out_t, sBuf, sizeof(sBuf));
	int ws = write(sockfd, sBuf, sLen);
	if (ws != sLen) 
	{
	    dfscli_log(DFS_LOG_WARN, "write err, ws: %d, sLen: %d", ws, sLen);
		
	    close(sockfd);
		
        return DFS_ERROR;
	}

	int pLen = 0;
	int rLen = recv(sockfd, &pLen, sizeof(int), MSG_PEEK);
	if (rLen < 0) 
	{
	    dfscli_log(DFS_LOG_WARN, "recv err, rLen: %d", rLen);
		
	    close(sockfd);
		
        return DFS_ERROR;
	}

	char *pNext = (char *)pool_alloc(dfs_cycle->pool, pLen);
	if (!pNext) 
	{
	    dfscli_log(DFS_LOG_WARN, "pool_alloc err, pLen: %d", pLen);
		
	    close(sockfd);
		
        return DFS_ERROR;
	}

	rLen = read(sockfd, pNext, pLen);
	if (rLen < 0) 
	{
	    dfscli_log(DFS_LOG_WARN, "read err, rLen: %d", rLen);
		
	    close(sockfd);

        return DFS_ERROR;
	}
	
	task_t in_t;
	bzero(&in_t, sizeof(task_t));
	task_decodefstr(pNext, rLen, &in_t);

    if (in_t.ret != DFS_OK) 
	{
		if (in_t.ret == KEY_NOTEXIST) 
		{
            dfscli_log(DFS_LOG_WARN, 
				"open err, file %s doesn't exist.", rw_ctx->src);
		}
		else if (in_t.ret == KEY_STATE_CREATING) 
		{
            dfscli_log(DFS_LOG_WARN, 
				"open err, file %s is creating.", rw_ctx->src);
		}
		else if (in_t.ret == NOT_FILE) 
		{
            dfscli_log(DFS_LOG_WARN, 
				"open err, path %s is not a file.", rw_ctx->src);
		}
		else if (in_t.ret == PERMISSION_DENY) 
		{
            dfscli_log(DFS_LOG_WARN, "open err, permission deny.");
		}
		else 
		{
            dfscli_log(DFS_LOG_WARN, "open err, ret: %d", in_t.ret);
		}

		close(sockfd);

        return DFS_ERROR;
	}
	else if (NULL != in_t.data && in_t.data_len > 0) 
	{
        memset(&resp_info, 0x00, sizeof(create_resp_info_t));
		memcpy(&resp_info, in_t.data, in_t.data_len);

		rw_ctx->nn_fd = sockfd;
		rw_ctx->blk_id = resp_info.blk_id;
		rw_ctx->blk_sz = resp_info.blk_sz;
		rw_ctx->namespace_id = resp_info.namespace_id;
		rw_ctx->dn_num = resp_info.dn_num;
		memcpy(rw_ctx->dn_ips, resp_info.dn_ips, sizeof(resp_info.dn_ips));
	}
	
    return DFS_OK;
}

static int dfs_read_blk(rw_context_t *rw_ctx)
{
    int dn_index = 0;
	int res = -1;

	int datafd = open(rw_ctx->dst, O_RDWR | O_CREAT | O_TRUNC, 0666);
	if (datafd < 0) 
	{
        dfscli_log(DFS_LOG_WARN, "open %s err, %s", 
			rw_ctx->dst, strerror(errno));
			
        return DFS_ERROR;
	}

	int sockfd = dfs_connect(rw_ctx->dn_ips[dn_index], DN_PORT);
	if (sockfd < 0) 
	{
		close(datafd);
		unlink(rw_ctx->dst);
		
	    return DFS_ERROR;
	}

	long fsize = rw_ctx->blk_sz;

	data_transfer_header_t header;
	memset(&header, 0x00, sizeof(data_transfer_header_t));

	header.op_type = OP_READ_BLOCK;
	header.namespace_id = rw_ctx->namespace_id;
	header.block_id = rw_ctx->blk_id;
	header.generation_stamp = 0;
	header.start_offset = 0;
	header.len = fsize;

	res = send(sockfd, &header, sizeof(data_transfer_header_t), 0);
	if (res < 0) 
	{
	    dfscli_log(DFS_LOG_WARN, "send header to %s err, %s", 
			rw_ctx->dn_ips[dn_index], strerror(errno));

		close(datafd);
		close(sockfd);

		unlink(rw_ctx->dst);
		
	    return DFS_ERROR;
	}

    data_transfer_header_rsp_t rsp;
	memset(&rsp, 0x00, sizeof(data_transfer_header_rsp_t));
	res = recv(sockfd, &rsp, sizeof(data_transfer_header_rsp_t), 0);
	if (res < 0 || (rsp.op_status != OP_STATUS_SUCCESS && rsp.err != DFS_OK)) 
	{
	    dfscli_log(DFS_LOG_WARN, "recv header rsp from %s err, %s", 
			rw_ctx->dn_ips[dn_index], strerror(errno));

		close(datafd);
		close(sockfd);

		unlink(rw_ctx->dst);
		
	    return DFS_ERROR;
	}

	if (do_recvfile_splice(sockfd, datafd, NULL, fsize) == DFS_ERROR) 
	{
	    close(datafd);
		close(sockfd);

		unlink(rw_ctx->dst);
		
	    return DFS_ERROR;
	}
	
	memset(&rsp, 0x00, sizeof(data_transfer_header_rsp_t));
	res = recv(sockfd, &rsp, sizeof(data_transfer_header_rsp_t), 0);
	if (res < 0 || (rsp.op_status != OP_STATUS_SUCCESS && rsp.err != DFS_OK)) 
	{
	    dfscli_log(DFS_LOG_WARN, "recv read done rsp from %s err, %s", 
			rw_ctx->dn_ips[dn_index], strerror(errno));
		
        close(datafd);
		close(sockfd);

		unlink(rw_ctx->dst);
		
	    return DFS_ERROR;
	}

	dfscli_log(DFS_LOG_INFO, "get file %s to local %s succesfully.", 
		rw_ctx->src, rw_ctx->dst);

	close(datafd);
	close(sockfd);

	return DFS_OK;
}

static int do_recvfile_splice(int fromfd, int tofd, 
	loff_t *offset, size_t count)
{
    int pipefd[2];
    long rc = 0, wc = 0, nread = 0, nwrite = 0, total = 0;
    loff_t off = 0;

    if (offset == NULL) 
    {
        offset = &off;
    }
    
    rc = pipe(pipefd);
    if (rc < 0) 
	{
        dfscli_log(DFS_LOG_WARN, "pipe err, %s", strerror(errno));
		
        return DFS_ERROR;
    }

    rc = count;
	
    while (rc > 0) 
	{
        /* 
         * {len} is bound upto 16384, because the process blocks forever
         * when setting large number to it. 
         */
        nread = splice(fromfd, NULL, pipefd[1], NULL, MIN(rc, 16384), 
            SPLICE_F_MOVE);
        if (nread < 0) 
		{
			dfscli_log(DFS_LOG_WARN, "splice(socket to pipe) err, %s", 
				strerror(errno));
			
            return DFS_ERROR;
        } 
		else if (nread == 0)
		{
		    dfscli_log(DFS_LOG_WARN, "no data to transfer");

		    return DFS_ERROR;
        }

        wc = nread;
		
        while (wc > 0) 
		{
            nwrite = splice(pipefd[0], NULL, tofd, offset, wc, SPLICE_F_MOVE);
            if (nwrite < 0) 
			{
				dfscli_log(DFS_LOG_WARN, "splice(pipe to file) err, %s", 
					strerror(errno));

				return DFS_ERROR;
            }
			
            wc -= nwrite;
        }
		
        rc -= nread;
        total += nread;
    }

    return total;
}

