#include "dfs_sys.h"

int sys_get_info(sys_info_t *sys_info)
{
    // get cpu info and set cacheline size
	uint64_t pages = 0;
        
    // pag esize
#if defined(_SC_PAGESIZE)
    sys_info->pagesize = sysconf(_SC_PAGESIZE);
#elif defined(_SC_PAGE_SIZE)
	sys_info->pagesize = sysconf(_SC_PAGE_SIZE);
#else
	sys_info->pagesize = 4096;
#endif
	if (sys_info->pagesize < 0) 
	{
		return DFS_FALSE;
    }

	sys_info->cpu_num =  sysconf(_SC_NPROCESSORS_ONLN);
	if (sys_info->cpu_num < 0) 
	{
		return DFS_FALSE;
    }
	
    // level1 cache
    sys_info->lv1_dcache_size = sysconf(_SC_LEVEL1_DCACHE_SIZE);
    if (sys_info->lv1_dcache_size < 0)
	{
    	return DFS_FALSE;
    }
	
    sys_info->lv1_dcacheline_size =
        sysconf(_SC_LEVEL1_DCACHE_LINESIZE);
    if (sys_info->lv1_dcacheline_size < 0) 
	{
    	return DFS_FALSE;
    }
	
    // level2 cache
    sys_info->lv2_cache_size = sysconf(_SC_LEVEL2_CACHE_SIZE);
    if (sys_info->lv2_cache_size < 0) 
	{
    	return DFS_FALSE;
    }
	
    sys_info->lv2_cacheline_size = 
        sysconf(_SC_LEVEL2_CACHE_LINESIZE);
    if (sys_info->lv2_cacheline_size < 0) 
	{
    	return DFS_FALSE;
    }

    pages = sysconf(_SC_PHYS_PAGES);

	sys_info->mem_num = pages * sys_info->pagesize;

    return DFS_OK;
}

