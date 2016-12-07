#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/time.h>
#include "dn_ns_service.h"
#include "dfs_types.h"
#include "dfs_task.h"
#include "dn_cycle.h"
#include "dn_time.h"
#include "dn_conf.h"

#define BUF_SZ 4096

unsigned long g_last_heartbeat = 0;

typedef struct recv_blk_report_s
{
    queue_t         que;
	int             num;
	pthread_mutex_t lock;
	pthread_cond_t  cond;
} recv_blk_report_t;

typedef struct blk_report_s
{
    queue_t         que;
	int             num;
	pthread_mutex_t lock;
	pthread_cond_t  cond;
} blk_report_t;

recv_blk_report_t g_recv_blk_report;
blk_report_t      g_blk_report;

static int ns_srv_init(char* ip, int port);
static int send_heartbeat(int sockfd);
static int receivedblock_report(int sockfd);
static int wait_to_work(int second);
static int block_report(int sockfd);
static int delete_blks(char *p, int len);

int dn_register(dfs_thread_t *thread)
{
    int sockfd = ns_srv_init(thread->ns_info.ip, thread->ns_info.port);
	if (sockfd < 0) 
	{
	    return DFS_ERROR;
	}

	task_t out_t;
	bzero(&out_t, sizeof(task_t));
	out_t.cmd = DN_REGISTER;
	strcpy(out_t.key, dfs_cycle->listening_ip);

	char sBuf[BUF_SZ] = "";
	int sLen = task_encode2str(&out_t, sBuf, sizeof(sBuf));
	int ws = write(sockfd, sBuf, sLen);
	if (ws != sLen) 
	{
	    dfs_log_error(dfs_cycle->error_log, DFS_LOG_FATAL, errno, 
			"write err, ws: %d, sLen: %d", ws, sLen);
		
	    close(sockfd);
		
        return DFS_ERROR;
	}

	int pLen = 0;
	int rLen = recv(sockfd, &pLen, sizeof(int), MSG_PEEK);
	if (rLen < 0) 
	{
	    dfs_log_error(dfs_cycle->error_log, DFS_LOG_FATAL, errno,
			"recv err, rLen: %d", rLen);
		
	    close(sockfd);
		
        return DFS_ERROR;
	}

	char *pNext = (char *)malloc(pLen);
	if (NULL == pNext) 
	{
	    dfs_log_error(dfs_cycle->error_log, DFS_LOG_FATAL, errno, 
			"malloc err, pLen: %d", pLen);
		
	    close(sockfd);
		
        return DFS_ERROR;
	}

	rLen = read(sockfd, pNext, pLen);
	if (rLen < 0) 
	{
	    dfs_log_error(dfs_cycle->error_log, DFS_LOG_FATAL, errno, 
			"read err, rLen: %d", rLen);
		
	    close(sockfd);

		free(pNext);
		pNext = NULL;
		
        return DFS_ERROR;
	}
	
	task_t in_t;
	bzero(&in_t, sizeof(task_t));
	task_decodefstr(pNext, rLen, &in_t);

    if (in_t.ret != DFS_OK) 
	{
		dfs_log_error(dfs_cycle->error_log, DFS_LOG_FATAL, 0, 
			"dn_register err, ret: %d", in_t.ret);
		
	    close(sockfd);

		free(pNext);
		pNext = NULL;
		
        return DFS_ERROR;
	}
	else if (NULL != in_t.data && in_t.data_len > 0) 
	{
	    thread->ns_info.namespaceID = *(int64_t *)in_t.data;
	}

	thread->ns_info.sockfd = sockfd;

	free(pNext);
	pNext = NULL;
	
    return DFS_OK;
}

static int ns_srv_init(char* ip, int port)
{
	int sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (-1 == sockfd) 
	{
	    dfs_log_error(dfs_cycle->error_log, DFS_LOG_FATAL, errno, 
			"socket() err");
		
	    return DFS_ERROR;
	}

	int reuse = 1;
	setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

	struct sockaddr_in servaddr;
	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(port);
	servaddr.sin_addr.s_addr = inet_addr(ip);

	int iRet = connect(sockfd, (struct sockaddr*)&servaddr, sizeof(servaddr));
	if (iRet < 0) 
	{
	    dfs_log_error(dfs_cycle->error_log, DFS_LOG_FATAL, errno, 
			"connect(%s: %d) err", ip, port);
		
	    return DFS_ERROR;
	}

	return sockfd;
}

int offer_service(dfs_thread_t *thread)
{
    conf_server_t *sconf = (conf_server_t *)dfs_cycle->sconf;
    int heartbeat_interval = sconf->heartbeat_interval;
	int block_report_interval = sconf->block_report_interval;

    struct timeval now;
	gettimeofday(&now, NULL);
	unsigned long now_time = now.tv_sec + now.tv_usec / (1000 * 1000);
	unsigned long diff = 0;
		
    while (thread->running) 
	{
	    if (diff >= heartbeat_interval) 
		{
		    g_last_heartbeat = now_time;
			
		    if (send_heartbeat(thread->ns_info.sockfd) != DFS_OK) 
			{
			    goto out;
			}
		}

        if (g_recv_blk_report.num > 0)
		{
            if (receivedblock_report(thread->ns_info.sockfd) != DFS_OK) 
		    {
                goto out;
		    }
		}

		if (g_blk_report.num > 0) 
		{
            if (block_report(thread->ns_info.sockfd) != DFS_OK) 
		    {
                goto out;
		    }
		}
		
        int ptime = heartbeat_interval - diff;
		int wtime = ptime > 0 ? ptime : heartbeat_interval;
		
		if (wtime > 0 && g_recv_blk_report.num == 0 && g_blk_report.num == 0) 
		{
		    g_last_heartbeat = now_time;
			
	        wait_to_work(wtime);
		}

		gettimeofday(&now, NULL);
		now_time = (now.tv_sec + now.tv_usec / (1000 * 1000));
	    diff = now_time - g_last_heartbeat;
	}

out:
	close(thread->ns_info.sockfd);
	thread->ns_info.sockfd = -1;
	thread->ns_info.namespaceID = -1;
	
    return DFS_ERROR;
}

static int send_heartbeat(int sockfd)
{
    task_t out_t;
	bzero(&out_t, sizeof(task_t));
	out_t.cmd = DN_HEARTBEAT;
	strcpy(out_t.key, dfs_cycle->listening_ip);

	char sBuf[BUF_SZ] = "";
	int sLen = task_encode2str(&out_t, sBuf, sizeof(sBuf));
	int ws = write(sockfd, sBuf, sLen);
	if (ws != sLen) 
	{
	    dfs_log_error(dfs_cycle->error_log, DFS_LOG_FATAL, errno, 
			"write err, ws: %d, sLen: %d", ws, sLen);

        return DFS_ERROR;
	}

	int pLen = 0;
	int rLen = recv(sockfd, &pLen, sizeof(int), MSG_PEEK);
	if (rLen < 0) 
	{
	    dfs_log_error(dfs_cycle->error_log, DFS_LOG_FATAL, errno,
			"read err, rLen: %d", rLen);
		
        return DFS_ERROR;
	}

	char *pNext = (char *)malloc(pLen);
	if (NULL == pNext) 
	{
		dfs_log_error(dfs_cycle->error_log, DFS_LOG_FATAL, errno,
			"malloc err, pLen: %d", pLen);
		
        return DFS_ERROR;
	}

	rLen = read(sockfd, pNext, pLen);
	if (rLen < 0) 
	{
	    dfs_log_error(dfs_cycle->error_log, DFS_LOG_FATAL, errno,
			"read err, rLen: %d", rLen);

		free(pNext);
		pNext = NULL;
		
        return DFS_ERROR;
	}

	task_t in_t;
	bzero(&in_t, sizeof(task_t));
	task_decodefstr(pNext, rLen, &in_t);

    if (in_t.ret != DFS_OK) 
	{
	    dfs_log_error(dfs_cycle->error_log, DFS_LOG_FATAL, 0, 
			"send_heartbeat err, ret: %d", in_t.ret);

		free(pNext);
		pNext = NULL;
		
        return DFS_ERROR;
	} 
	else if (NULL != in_t.data && in_t.data_len > 0) 
	{
	    delete_blks((char *)in_t.data, in_t.data_len);
	}

	dfs_log_error(dfs_cycle->error_log, DFS_LOG_INFO, 0, 
		"send_heartbeat ok, ret: %d", in_t.ret);

	free(pNext);
	pNext = NULL;
	
    return DFS_OK;
}

static int receivedblock_report(int sockfd)
{
    queue_t           *cur = NULL;
	block_info_t      *blk = NULL;
	report_blk_info_t  rbi;

	pthread_mutex_lock(&g_recv_blk_report.lock);
    
	cur = queue_head(&g_recv_blk_report.que);
    queue_remove(cur);
    blk = queue_data(cur, block_info_t, me);
	g_recv_blk_report.num--;
    
    pthread_mutex_unlock(&g_recv_blk_report.lock);
	
    task_t out_t;
	bzero(&out_t, sizeof(task_t));
	out_t.cmd = DN_RECV_BLK_REPORT;
	strcpy(out_t.key, dfs_cycle->listening_ip);

	memset(&rbi, 0x00, sizeof(report_blk_info_t));
	rbi.blk_id = blk->id;
	rbi.blk_sz = blk->size;
	strcpy(rbi.dn_ip, dfs_cycle->listening_ip);

	out_t.data_len = sizeof(report_blk_info_t);
	out_t.data = &rbi;

	char sBuf[BUF_SZ] = "";
	int sLen = task_encode2str(&out_t, sBuf, sizeof(sBuf));
	int ws = write(sockfd, sBuf, sLen);
	if (ws != sLen) 
	{
	    dfs_log_error(dfs_cycle->error_log, DFS_LOG_FATAL, errno, 
			"write err, ws: %d, sLen: %d", ws, sLen);

        return DFS_ERROR;
	}

	char rBuf[BUF_SZ] = "";
	int rLen = read(sockfd, rBuf, sizeof(rBuf));
	if (rLen < 0) 
	{
	    dfs_log_error(dfs_cycle->error_log, DFS_LOG_FATAL, errno,
			"read err, rLen: %d", rLen);
		
        return DFS_ERROR;
	}
	
	task_t in_t;
	bzero(&in_t, sizeof(task_t));
	task_decodefstr(rBuf, rLen, &in_t);

    if (in_t.ret != DFS_OK) 
	{
	    dfs_log_error(dfs_cycle->error_log, DFS_LOG_FATAL, 0, 
			"receivedblock_report err, ret: %d", in_t.ret);
		
        return DFS_ERROR;
	}

	dfs_log_error(dfs_cycle->error_log, DFS_LOG_INFO, 0, 
		"receivedblock_report ok, ret: %d", in_t.ret);
	
    return DFS_OK;
}

static int wait_to_work(int second)
{
    struct timespec timer;
	struct timeval now;
	gettimeofday(&now, NULL);
	timer.tv_sec = now.tv_sec + second;
	timer.tv_nsec = now.tv_usec * 1000;

	pthread_mutex_lock(&g_recv_blk_report.lock);
    
    while (g_recv_blk_report.num == 0) 
	{   
        int rs = pthread_cond_timedwait(&g_recv_blk_report.cond, 
			&g_recv_blk_report.lock, &timer); 
		if (rs == ETIMEDOUT) 
		{
            break;
		}
    }
    
	pthread_mutex_unlock(&g_recv_blk_report.lock);
	
    return DFS_OK;
}

int blk_report_queue_init()
{
    queue_init(&g_recv_blk_report.que);
	g_recv_blk_report.num = 0;

	pthread_mutex_init(&g_recv_blk_report.lock, NULL);
	pthread_cond_init(&g_recv_blk_report.cond, NULL);

	queue_init(&g_blk_report.que);
	g_blk_report.num = 0;

	pthread_mutex_init(&g_blk_report.lock, NULL);
	pthread_cond_init(&g_blk_report.cond, NULL);
	
    return DFS_OK;
}

int blk_report_queue_release()
{
    pthread_mutex_destroy(&g_recv_blk_report.lock);
	pthread_cond_destroy(&g_recv_blk_report.cond);
	g_recv_blk_report.num = 0;

	pthread_mutex_destroy(&g_blk_report.lock);
	pthread_cond_destroy(&g_blk_report.cond);
	g_blk_report.num = 0;
	
    return DFS_OK;
}

int notify_nn_receivedblock(block_info_t *blk)
{
    pthread_mutex_lock(&g_recv_blk_report.lock);
    
    queue_insert_tail(&g_recv_blk_report.que, &blk->me);
	g_recv_blk_report.num++;

	pthread_cond_signal(&g_recv_blk_report.cond);
    
    pthread_mutex_unlock(&g_recv_blk_report.lock);
	
    return DFS_OK;
}

static int block_report(int sockfd)
{
    queue_t           *cur = NULL;
	block_info_t      *blk = NULL;
	report_blk_info_t  rbi;

	pthread_mutex_lock(&g_blk_report.lock);
    
	cur = queue_head(&g_blk_report.que);
    queue_remove(cur);
    blk = queue_data(cur, block_info_t, me);
	g_blk_report.num--;
    
    pthread_mutex_unlock(&g_blk_report.lock);
	
    task_t out_t;
	bzero(&out_t, sizeof(task_t));
	out_t.cmd = DN_BLK_REPORT;
	strcpy(out_t.key, dfs_cycle->listening_ip);

	memset(&rbi, 0x00, sizeof(report_blk_info_t));
	rbi.blk_id = blk->id;
	rbi.blk_sz = blk->size;
	strcpy(rbi.dn_ip, dfs_cycle->listening_ip);

	out_t.data_len = sizeof(report_blk_info_t);
	out_t.data = &rbi;

	char sBuf[BUF_SZ] = "";
	int sLen = task_encode2str(&out_t, sBuf, sizeof(sBuf));
	int ws = write(sockfd, sBuf, sLen);
	if (ws != sLen) 
	{
	    dfs_log_error(dfs_cycle->error_log, DFS_LOG_FATAL, errno, 
			"write err, ws: %d, sLen: %d", ws, sLen);

        return DFS_ERROR;
	}

	char rBuf[BUF_SZ] = "";
	int rLen = read(sockfd, rBuf, sizeof(rBuf));
	if (rLen < 0) 
	{
	    dfs_log_error(dfs_cycle->error_log, DFS_LOG_FATAL, errno,
			"read err, rLen: %d", rLen);
		
        return DFS_ERROR;
	}
	
	task_t in_t;
	bzero(&in_t, sizeof(task_t));
	task_decodefstr(rBuf, rLen, &in_t);

    if (in_t.ret != DFS_OK) 
	{
	    dfs_log_error(dfs_cycle->error_log, DFS_LOG_FATAL, 0, 
			"block_report err, ret: %d", in_t.ret);
		
        return DFS_ERROR;
	}

	dfs_log_error(dfs_cycle->error_log, DFS_LOG_INFO, 0, 
		"block_report ok, ret: %d", in_t.ret);
	
    return DFS_OK;
}

int notify_blk_report(block_info_t *blk)
{
    pthread_mutex_lock(&g_blk_report.lock);
    
    queue_insert_tail(&g_blk_report.que, &blk->me);
	g_blk_report.num++;
    
    pthread_mutex_unlock(&g_blk_report.lock);
	
    return DFS_OK;
}

static int delete_blks(char *p, int len)
{
    uint64_t blk_id = 0;
	int      pLen = sizeof(uint64_t);
	
    while (len > 0) 
	{
        memcpy(&blk_id, p, pLen);

		block_object_del(blk_id);

		p += pLen;
		len -= pLen;
	}
	
    return DFS_OK;
}

