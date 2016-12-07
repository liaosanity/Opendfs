#include <stdio.h>

#include "dfs_lock.h"
#include "dn_time.h"

#define dfs_time_trylock(lock)  (*(lock) == 0 && CAS(lock, 0, 1))
#define dfs_time_unlock(lock)    *(lock) = 0
#define dfs_memory_barrier()    __asm__ volatile ("" ::: "memory")

#define CACHE_TIME_SLOT    64
static uint32_t            slot;
static uchar_t             dfs_cache_log_time[CACHE_TIME_SLOT]
                               [sizeof("1970/09/28 12:00:00")];

_xvolatile rb_msec_t       dfs_current_msec;
_xvolatile string_t        dfs_err_log_time;
_xvolatile struct timeval *dfs_time;
 struct timeval cur_tv;
_xvolatile uint64_t time_lock = 0;

int time_init(void)
{
    dfs_err_log_time.len = sizeof("1970/09/28 12:00:00.xxxx") - 1;
    
    dfs_time = &cur_tv;
    time_update();
	
    return DFS_OK;
}

void time_update(void)
{
    struct timeval  tv;
    struct tm       tm;
    time_t          sec;
    uint32_t        msec = 0;
    uchar_t        *p0 = NULL;
    rb_msec_t       nmsec;

    if (!dfs_time_trylock(&time_lock)) 
	{
        return;
    }

    time_gettimeofday(&tv);
    sec = tv.tv_sec;
    msec = tv.tv_usec / 1000;
	
	dfs_current_msec = (rb_msec_t) sec * 1000 + msec;
	nmsec = dfs_time->tv_sec * 1000 + dfs_time->tv_usec/1000; 
	if ( dfs_current_msec - nmsec < 10) 
	{
		dfs_time_unlock(&time_lock);
		
		return;
    }
	slot++;
	slot ^=(CACHE_TIME_SLOT - 1);
    dfs_time->tv_sec = tv.tv_sec;
	dfs_time->tv_usec = tv.tv_usec;
	
    time_localtime(sec, &tm);
    p0 = &dfs_cache_log_time[slot][0];
	sprintf((char*)p0, "%4d/%02d/%02d %02d:%02d:%02d.%04d",
        tm.tm_year, tm.tm_mon,
        tm.tm_mday, tm.tm_hour,
        tm.tm_min, tm.tm_sec, msec);
    dfs_memory_barrier();
    
    dfs_err_log_time.data = p0;

    dfs_time_unlock(&time_lock);
}

_xvolatile string_t *time_logstr()
{
    return &dfs_err_log_time;
}

rb_msec_t time_curtime(void)
{
    return dfs_current_msec;
}

