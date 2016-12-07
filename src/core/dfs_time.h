#ifndef DFS_TIME_H
#define DFS_TIME_H

#include "dfs_types.h"

// rfc1123:GMT TIME: Mon, 28 Sep 1970 06:00:00 GMT
#define TIME_RFC1123_SIZE       sizeof("Mon, 28 Sep 1970 06:00:00 GMT") - 1
#define TIME_RFC822_SIZE        sizeof("Mon, 28 Sep 1970 06:00:00") - 1
#define TIME_RFC850_SIZE        sizeof("Tuesday, 10-Dec-02 23:50:13") - 1
#define TIME_ISO_SIZE           sizeof("Tue Dec 10 23:50:13 2002") - 1
#define TIME_MIN_SIZE           sizeof("10-Dec-02 23:50:13") - 1

#define time_gettimeofday(tp)   gettimeofday(tp, NULL);
#define time_msleep(ms)         usleep(ms * 1000)
#define time_sleep(s)           sleep(s)
#define time_mktime(s)          mktime(s)

#define time_timezone(isdst)    (-(isdst ? timezone + 3600 : timezone) / 60)

enum 
{
    TIME_FORMAT_NONE,
    TIME_FORMAT_RFC822,
    TIME_FORMAT_RFC850,
    TIME_FORMAT_ISO,
};

enum 
{
    TIME_MONTH_JANUARY = 0,
    TIME_MONTH_FEBRUARY,
    TIME_MONTH_MARCH,
    TIME_MONTH_APRIL,
    TIME_MONTH_MAY,
    TIME_MONTH_JUNE,
    TIME_MONTH_JULY,
    TIME_MONTH_AUGUST,
    TIME_MONTH_SEPTEMBER,
    TIME_MONTH_OCTOBER,
    TIME_MONTH_NOVEMBER,
    TIME_MONTH_DECEMBER,
};

int      time_monthtoi(const char *s);
void     time_localtime(time_t s, struct tm *tm);
void     time_gmtime(time_t t, struct tm *tp);
uchar_t *time_to_http_time(uchar_t *buf, time_t t);
uchar_t *time_to_http_cookie_time(uchar_t *buf, time_t t);

#endif

