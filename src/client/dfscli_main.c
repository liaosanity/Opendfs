#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/resource.h>
#include <sys/syscall.h>
#include <fcntl.h>
#include <unistd.h>
#include <pwd.h>
#include <grp.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include "dfscli_main.h"
#include "dfs_string.h"
#include "fs_permission.h"
#include "nn_file_index.h"
#include "config.h"
#include "dfscli_conf.h"
#include "dfscli_put.h"
#include "dfscli_get.h"

#define INVALID_SYMBOLS_IN_PATH "\\:*?\"<>|"
#define MY_LOG_RAW (1 << 10) // Modifier to log without timestamp

#define DEFAULT_CONF_FILE PREFIX"/etc/dfscli.conf"

string_t config_file;

static void log_raw(uint32_t level, const char *msg);
static void help(int argc, char **argv);
static int dfscli_mkdir(char *path);
static int dfscli_rmr(char *path);
static int dfscli_ls(char *path);
static int showDirsFiles(char *p, int len);
static int getTimeStr(uint64_t msec, char *str, int len);
static int isPathValid(char *path);
static int getValidPath(char *src, char *dst);
static int dfscli_rm(char *path);

int dfscli_daemon()
{
    return 0;
}

void dfscli_log(int level, const char *fmt, ...)
{
    va_list        ap;    
	char           msg[LOGMSG_LEN] = "";
	conf_server_t *sconf = NULL;

	sconf = (conf_server_t *)dfs_cycle->sconf;
	
	if ((level & 0xff) > sconf->log_level)     
	{        
	    return;    
	}    

	va_start(ap, fmt);    
	vsnprintf(msg, sizeof(msg), fmt, ap);    
	va_end(ap);    

	log_raw(level, msg);
}

static void log_raw(uint32_t level, const char *msg) 
{    
    const char    *c = "#FEAWNICED";
	conf_server_t *sconf = NULL;

	sconf = (conf_server_t *)dfs_cycle->sconf;
	
	int rawmode = (level & MY_LOG_RAW);    

	level &= 0xff; // clear flags    
	if (level > sconf->log_level)     
	{        
	    return;    
	}    

	FILE *fp = (0 == sconf->error_log.len) 
		? stdout : fopen((const char *)sconf->error_log.data, "a");    
	if (NULL == fp)     
	{        
	    return;    
	}    

    if (rawmode)     
	{        
	    fprintf(fp, "%s", msg);    
	}     
	else    
	{        
	    int off = 0;        
		char buf[64] = "";        
		struct timeval tv;        

		gettimeofday(&tv, NULL);        
		off = strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S.", 
			localtime(&tv.tv_sec));        
		snprintf(buf + off, sizeof(buf) - off, "%03d", (int)tv.tv_usec / 1000);                

		switch (level)        
		{        
		case DFS_LOG_WARN:            
			fprintf(fp, "%s[%ld] %s [%c] %s%s\n", ANSI_YELLOW, 
				syscall(__NR_gettid), buf, c[level], msg, ANSI_RESET);
			break;

		case DFS_LOG_ERROR:
			fprintf(fp, "%s[%ld] %s [%c] %s%s\n", ANSI_RED, 
				syscall(__NR_gettid), buf, c[level], msg, ANSI_RESET);
			break;

		default:
			fprintf(fp, "[%ld] %s [%c] %s\n", 
				syscall(__NR_gettid), buf, c[level], msg);
		}
	}        

	fflush(fp);    

	if (0 != sconf->error_log.len)     
	{        
        fclose(fp);    
	}
}

static void help(int argc, char **argv)
{		    
    fprintf(stderr, "Usage: %s cmd...\n"
		"\t -mkdir <path> \n"
		"\t -rmr <path> \n"
		"\t -ls <path> \n"
		"\t -put <local path> <remote path> \n"
		"\t -get <remote path> <local path> \n"
		"\t -rm <path> \n", 
		argv[0]);
}

int main(int argc, char **argv)
{
    if (argc < 3) 
	{
	    help(argc, argv);

	    return DFS_ERROR;
	}

    int            ret = DFS_OK;
	cycle_t       *cycle = NULL;
    conf_server_t *sconf = NULL;
	char           cmd[16] = {0};
	char           path[PATH_LEN] = {0};

	cycle = cycle_create();

	if (config_file.data == NULL) 
	{
        config_file.data = (uchar_t *)strndup(DEFAULT_CONF_FILE,
            strlen(DEFAULT_CONF_FILE));
        config_file.len = strlen(DEFAULT_CONF_FILE);
    }

	if ((ret = cycle_init(cycle)) != DFS_OK) 
	{
        fprintf(stderr, "cycle_init fail\n");
		
        goto out;
    }

    strcpy(cmd, argv[1]);

    if (strlen(argv[2]) > (int)PATH_LEN) 
	{
        dfscli_log(DFS_LOG_WARN, "path's len is greater than %d", 
			(int)PATH_LEN);

		goto out;
	}
	
	strcpy(path, argv[2]);

	if (0 == strncmp(cmd, "-mkdir", strlen("-mkdir"))) 
	{
	    if (!isPathValid(path)) 
	    {
		    dfscli_log(DFS_LOG_WARN, 
				"path[%s] is invalid, these symbols[%s] can't use in the path", 
				path, INVALID_SYMBOLS_IN_PATH);
		
		    goto out;
	    }

        char vPath[PATH_LEN] = {0};
		getValidPath(path, vPath);

        dfscli_mkdir(vPath);
	}
	else if (0 == strncmp(cmd, "-rmr", strlen("-rmr"))) 
	{
	    // check path's pattern
	    
	    char vPath[PATH_LEN] = {0};
		getValidPath(path, vPath);
		
        dfscli_rmr(vPath);
	}
	else if (0 == strncmp(cmd, "-ls", strlen("-ls")))
	{
	    // check path's pattern

		char vPath[PATH_LEN] = {0};
		getValidPath(path, vPath);
		
        dfscli_ls(vPath);
	}
	else if (4 == argc && 0 == strncmp(cmd, "-put", strlen("-put"))) 
	{
        char tmp[PATH_LEN] = {0};
		strcpy(tmp, argv[3]);

		char src[PATH_LEN] = {0};
		getValidPath(path, src);

		char dst[PATH_LEN] = {0};
		getValidPath(tmp, dst);

		dfscli_put(src, dst);
	}
	else if (4 == argc && 0 == strncmp(cmd, "-get", strlen("-get"))) 
	{
        //char tmp[PATH_LEN] = {0};
		//strcpy(tmp, argv[3]);

		char src[PATH_LEN] = {0};
		getValidPath(path, src);

		char dst[PATH_LEN] = {0};
		//getValidPath(tmp, dst);
		strcpy(dst, argv[3]);

		dfscli_get(src, dst);
	}
	else if (0 == strncmp(cmd, "-rm", strlen("-rm"))) 
	{
        // check path's pattern

		char vPath[PATH_LEN] = {0};
		getValidPath(path, vPath);
		
        dfscli_rm(vPath);
	}
	else 
	{
	    help(argc, argv);
	}

out:
	if (config_file.data) 
	{
        free(config_file.data);
        config_file.len = 0;
    }
	
    if (cycle) 
	{
        cycle_free(cycle);
		cycle = NULL;
    }
	
    return ret;
}

int dfs_connect(char* ip, int port)
{
	int sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (-1 == sockfd) 
	{
	    dfscli_log(DFS_LOG_WARN, "socket() err: %s", strerror(errno));
		
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
	    dfscli_log(DFS_LOG_WARN, "connect to %s:%d err: %s", 
			ip, port, strerror(errno));
		
	    return DFS_ERROR;
	}

	return sockfd;
}

void keyEncode(uchar_t *path, uchar_t *key)
{
    string_t src;
    string_set(src, path);

    string_t dst;
    string_set(dst, key);

    string_base64_encode(&dst, &src);
}

void keyDecode(uchar_t *key, uchar_t *path)
{
	string_t src;
    string_set(src, key);

    string_t dst;
    string_set(dst, path);

    string_base64_decode(&dst, &src);
}

void getUserInfo(task_t *out_t)
{
    struct passwd *passwd;
	passwd = getpwuid(getuid());
	strcpy(out_t->user, passwd->pw_name);
	struct group *group;
	group = getgrgid(passwd->pw_gid);
	strcpy(out_t->group, group->gr_name);
}

static int dfscli_mkdir(char *path)
{
    conf_server_t *sconf = NULL;
    server_bind_t *nn_addr = NULL;

	sconf = (conf_server_t *)dfs_cycle->sconf;
	nn_addr = (server_bind_t *)sconf->namenode_addr.elts;
	
    int sockfd = dfs_connect((char *)nn_addr[0].addr.data, nn_addr[0].port);
	if (sockfd < 0) 
	{
	    return DFS_ERROR;
	}
	
	task_t out_t;
	bzero(&out_t, sizeof(task_t));
	out_t.cmd = NN_MKDIR;
	keyEncode((uchar_t *)path, (uchar_t *)out_t.key);

	getUserInfo(&out_t);

	out_t.permission = 755;

	char sBuf[BUF_SZ] = "";
	int sLen = task_encode2str(&out_t, sBuf, sizeof(sBuf));
	int ws = write(sockfd, sBuf, sLen);
	if (ws != sLen) 
	{
	    dfscli_log(DFS_LOG_WARN, "write err, ws: %d, sLen: %d", ws, sLen);
		
	    close(sockfd);
		
        return DFS_ERROR;
	}

	char rBuf[BUF_SZ] = "";
	int rLen = read(sockfd, rBuf, sizeof(rBuf));
	if (rLen < 0) 
	{
	    dfscli_log(DFS_LOG_WARN, "read err, rLen: %d", rLen);
		
	    close(sockfd);
		
        return DFS_ERROR;
	}
	
	task_t in_t;
	bzero(&in_t, sizeof(task_t));
	task_decodefstr(rBuf, rLen, &in_t);

    if (in_t.ret != DFS_OK) 
	{
	    if (in_t.ret == KEY_EXIST) 
		{
            dfscli_log(DFS_LOG_WARN, "mkdir err, path %s is exist.", path);
		}
		else if (in_t.ret == NOT_DIRECTORY) 
		{
            dfscli_log(DFS_LOG_WARN, 
				"mkdir err, parent path is not a directory.");
		} 
		else if (in_t.ret == PERMISSION_DENY) 
		{
            dfscli_log(DFS_LOG_WARN, "mkdir err, permission deny.");
		}
		else 
		{
            dfscli_log(DFS_LOG_WARN, "mkdir err, ret: %d", in_t.ret);
		}
	}

	close(sockfd);
	
    return DFS_OK;
}

static int dfscli_rmr(char *path)
{
    conf_server_t *sconf = NULL;
    server_bind_t *nn_addr = NULL;

	sconf = (conf_server_t *)dfs_cycle->sconf;
	nn_addr = (server_bind_t *)sconf->namenode_addr.elts;
	
    int sockfd = dfs_connect((char *)nn_addr[0].addr.data, nn_addr[0].port);
	if (sockfd < 0) 
	{
	    return DFS_ERROR;
	}
	
	task_t out_t;
	bzero(&out_t, sizeof(task_t));
	out_t.cmd = NN_RMR;
	keyEncode((uchar_t *)path, (uchar_t *)out_t.key);

	getUserInfo(&out_t);

	char sBuf[BUF_SZ] = "";
	int sLen = task_encode2str(&out_t, sBuf, sizeof(sBuf));
	int ws = write(sockfd, sBuf, sLen);
	if (ws != sLen) 
	{
	    dfscli_log(DFS_LOG_WARN, "write err, ws: %d, sLen: %d", ws, sLen);
		
	    close(sockfd);
		
        return DFS_ERROR;
	}

	char rBuf[BUF_SZ] = "";
	int rLen = read(sockfd, rBuf, sizeof(rBuf));
	if (rLen < 0) 
	{
	    dfscli_log(DFS_LOG_WARN, "read err, rLen: %d", rLen);
		
	    close(sockfd);
		
        return DFS_ERROR;
	}
	
	task_t in_t;
	bzero(&in_t, sizeof(task_t));
	task_decodefstr(rBuf, rLen, &in_t);

    if (in_t.ret != DFS_OK) 
	{
        if (in_t.ret == NOT_DIRECTORY) 
		{
            dfscli_log(DFS_LOG_WARN, 
				"rmr err, the target is a file, you should use -rm instead.");
		}
		else if (in_t.ret == KEY_NOTEXIST) 
		{
            dfscli_log(DFS_LOG_WARN, "rmr err, path %s doesn't exist.", path);
		}
		else if (in_t.ret == PERMISSION_DENY) 
		{
            dfscli_log(DFS_LOG_WARN, "rmr err, permission deny.");
		}
		else 
		{
            dfscli_log(DFS_LOG_WARN, "rmr err, ret: %d", in_t.ret);
		}
	}

	close(sockfd);
	
    return DFS_OK;
}

static int dfscli_ls(char *path)
{
    conf_server_t *sconf = NULL;
    server_bind_t *nn_addr = NULL;

	sconf = (conf_server_t *)dfs_cycle->sconf;
	nn_addr = (server_bind_t *)sconf->namenode_addr.elts;
	
    int sockfd = dfs_connect((char *)nn_addr[0].addr.data, nn_addr[0].port);
	if (sockfd < 0) 
	{
	    return DFS_ERROR;
	}
	
	task_t out_t;
	bzero(&out_t, sizeof(task_t));
	out_t.cmd = NN_LS;
	keyEncode((uchar_t *)path, (uchar_t *)out_t.key);

	getUserInfo(&out_t);

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

	char *pNext = (char *)malloc(pLen);
	if (NULL == pNext) 
	{
	    dfscli_log(DFS_LOG_WARN, "malloc err, pLen: %d", pLen);
		
	    close(sockfd);
		
        return DFS_ERROR;
	}

	rLen = read(sockfd, pNext, pLen);
	if (rLen < 0) 
	{
	    dfscli_log(DFS_LOG_WARN, "read err, rLen: %d", rLen);
		
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
		if (in_t.ret == KEY_NOTEXIST) 
		{
            dfscli_log(DFS_LOG_WARN, "ls err, path %s doesn't exist.", path);
		}
		else if (in_t.ret == PERMISSION_DENY) 
		{
            dfscli_log(DFS_LOG_WARN, "ls err, permission deny.");
		}
		else 
		{
            dfscli_log(DFS_LOG_WARN, "ls err, ret: %d", in_t.ret);
		}
	}
	else if (NULL != in_t.data && in_t.data_len > 0) 
	{
        showDirsFiles((char *)in_t.data, in_t.data_len);
	}

	close(sockfd);

	free(pNext);
	pNext = NULL;
	
    return DFS_OK;
}

static int showDirsFiles(char *p, int len)
{
    fi_inode_t fii;
	int fiiLen = sizeof(fi_inode_t);
	uchar_t permission[16] = "";
	char mtime[64] = "";
	uchar_t path[PATH_LEN] = "";

	while (len > 0) 
	{
	    bzero(&fii, sizeof(fi_inode_t));
	    memcpy(&fii, p, fiiLen);

		printf("%s", fii.is_directory ? "d" : "-");

        memset(permission, 0x00, sizeof(permission));
		get_permission(fii.permission, permission);
		printf("%s", permission);

		if (fii.is_directory) 
		{
		    printf("    -");
		}
		else 
		{
		    printf("    %d", fii.blk_replication);
		}

		printf(" %s %s    %ld", fii.owner, fii.group, fii.length);

        memset(mtime, 0x00, sizeof(mtime));
		getTimeStr(fii.modification_time, mtime, sizeof(mtime));
		printf(" %s", mtime);

        memset(path, 0x00, sizeof(path));
		keyDecode((uchar_t *)fii.key, path);
		printf(" %s\n", path);

		p += fiiLen;
		len -= fiiLen;
	}
	
    return DFS_OK;
}

static int getTimeStr(uint64_t msec, char *str, int len)
{
    time_t t;
    struct tm *p;

	t = msec / 1000 + 28800;
	p = gmtime(&t);
	strftime(str, len, "%Y-%m-%d %H:%M:%S", p);
	
    return DFS_OK;
}

static int isPathValid(char *path)
{
    char *p = path;
	
    while (*p != '\000') 
	{
	    if (*p == '\\' || *p == ':' || *p == '*' || *p == '?' || *p == '"' 
			|| *p == '<' || *p == '>' || *p == '|') 
		{
			return DFS_FALSE;
		}

		p++;
	}
		
    return DFS_TRUE;
}

static int getValidPath(char *src, char *dst)
{
    char *s = src;
	char *d = dst;
	char l = '/';

	if (0 == strcmp(src, "/") || 0 == strcmp(src, "//")) 
	{
	    strcpy(dst, "/");
		
        return DFS_OK;
	}

	while (*s != '\000') 
	{
	    if (l == '/' && *s == '/') 
		{
			l = *s++;

			continue;
		}

        if (l == '/') 
		{
		    *d++ = l;
		} 

		*d++ = *s++;
		l = *s;
	}

	if (l == '/') 
	{
	    *(d--) = '\000';
	}
	
    return DFS_OK;
}

static int dfscli_rm(char *path)
{
    conf_server_t *sconf = NULL;
    server_bind_t *nn_addr = NULL;

	sconf = (conf_server_t *)dfs_cycle->sconf;
	nn_addr = (server_bind_t *)sconf->namenode_addr.elts;
	
    int sockfd = dfs_connect((char *)nn_addr[0].addr.data, nn_addr[0].port);
	if (sockfd < 0) 
	{
	    return DFS_ERROR;
	}
	
	task_t out_t;
	bzero(&out_t, sizeof(task_t));
	out_t.cmd = NN_RM;
	keyEncode((uchar_t *)path, (uchar_t *)out_t.key);

	getUserInfo(&out_t);

	char sBuf[BUF_SZ] = "";
	int sLen = task_encode2str(&out_t, sBuf, sizeof(sBuf));
	int ws = write(sockfd, sBuf, sLen);
	if (ws != sLen) 
	{
	    dfscli_log(DFS_LOG_WARN, "write err, ws: %d, sLen: %d", ws, sLen);
		
	    close(sockfd);
		
        return DFS_ERROR;
	}

	char rBuf[BUF_SZ] = "";
	int rLen = read(sockfd, rBuf, sizeof(rBuf));
	if (rLen < 0) 
	{
	    dfscli_log(DFS_LOG_WARN, "read err, rLen: %d", rLen);
		
	    close(sockfd);
		
        return DFS_ERROR;
	}
	
	task_t in_t;
	bzero(&in_t, sizeof(task_t));
	task_decodefstr(rBuf, rLen, &in_t);

    if (in_t.ret != DFS_OK) 
	{
        if (in_t.ret == NOT_FILE) 
		{
            dfscli_log(DFS_LOG_WARN, 
				"rm err, the target is a directory, you should use -rmr instead.");
		}
		else if (in_t.ret == KEY_NOTEXIST) 
		{
            dfscli_log(DFS_LOG_WARN, "rm err, path %s doesn't exist.", path);
		}
		else if (in_t.ret == PERMISSION_DENY) 
		{
            dfscli_log(DFS_LOG_WARN, "rm err, permission deny.");
		}
		else 
		{
            dfscli_log(DFS_LOG_WARN, "rm err, ret: %d", in_t.ret);
		}
	}

	close(sockfd);
	
    return DFS_OK;
}

