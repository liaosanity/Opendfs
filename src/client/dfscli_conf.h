#ifndef DN_CONF_H
#define DN_CONF_H

#include "dfs_types.h"
#include "dfs_array.h"
#include "dfs_conf.h"

typedef struct conf_server_s conf_server_t;

struct conf_server_s 
{
    int      daemon;
	array_t  namenode_addr;
    string_t error_log;
    uint32_t log_level;
	uint32_t recv_buff_len;
    uint32_t send_buff_len;
	uint64_t blk_sz;
	short    blk_rep;
};

conf_object_t *get_dn_conf_object(void);

#define DEF_RBUFF_LEN          64 * 1024
#define DEF_SBUFF_LEN          64 * 1024
#define DEF_MMAX_TQUEUE_LEN    1000

#define set_def_string(key, value) do { \
    if (!(key)->len) { \
        (key)->data = (uchar_t *)(value); \
        (key)->len = sizeof(value); \
    }\
} while (0)

#define set_def_int(key, value) do { \
    if ((key) == CONF_SIZE_NOT_SET) { \
        (key) = value; \
    } \
} while (0)

#define set_def_time(key, value) do { \
    if ((key) == CONF_TIME_T_NOT_SET) { \
        (key) = value;\
    }\
} while (0)

#endif

