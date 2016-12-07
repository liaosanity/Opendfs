#ifndef DN_TIME_H
#define DN_TIME_H

#include "dfs_time.h"
#include "dfs_string.h"

#define time_timeofday() (struct timeval *) dfs_time

extern volatile string_t        dfs_err_log_time;
extern volatile rb_msec_t       dfs_current_msec;
extern volatile struct timeval *dfs_time;

int       time_init(void);
void      time_update(void);
_xvolatile string_t *time_logstr();
rb_msec_t time_curtime(void);

#endif

