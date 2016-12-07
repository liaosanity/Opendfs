#ifndef DFS_SYS_H
#define DFS_SYS_H

#include "dfs_types.h"

typedef struct sys_info_s sys_info_t;

struct sys_info_s
{
	long pagesize;
	long cpu_num;
	long lv1_dcache_size;
	long lv1_dcacheline_size;
	long lv2_cache_size;
	long lv2_cacheline_size;
	long mem_num;	// total memory
};

int sys_get_info(sys_info_t *sys_info);

#endif

