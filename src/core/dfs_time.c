#include "dfs_time.h"
#include "dfs_string.h"
#include "dfs_memory_pool.h"
#include "dfs_memory.h"

static char *week[] = { (char *)"Sun", (char *)"Mon", (char *)"Tue", 
    (char *)"Wed", (char *)"Thu", (char *)"Fri", (char *)"Sat" };
static char *months[] = { (char *)"Jan", (char *)"Feb", (char *)"Mar", 
    (char *)"Apr", (char *)"May", (char *)"Jun", (char *)"Jul", 
    (char *)"Aug", (char *)"Sep", (char *)"Oct", (char *)"Nov", 
    (char *)"Dec", NULL };

void time_localtime(time_t s, struct tm *tm)
{
    (void) localtime_r(&s, tm);
    tm->tm_mon++;
    tm->tm_year += 1900;
}

uchar_t * time_to_http_time(uchar_t *buf, time_t t)
{
    struct tm tm;

    time_gmtime(t, &tm);
	
    return string_xxsprintf(buf, "%s, %02d %s %4d %02d:%02d:%02d GMT",
        week[tm.tm_wday],
        tm.tm_mday,
        months[tm.tm_mon - 1],
        tm.tm_year,
        tm.tm_hour,
        tm.tm_min,
        tm.tm_sec);
}

uchar_t * time_to_http_cookie_time(uchar_t *buf, time_t t)
{
    struct tm  tm;

    time_gmtime(t, &tm);
	
    /*
     * Netscape 3.x does not understand 4-digit years at all and
     * 2-digit years more than "37"
     */
    return string_xxsprintf(buf,
        (tm.tm_year > 2037) ?
        "%s, %02d-%s-%d %02d:%02d:%02d GMT":
        "%s, %02d-%s-%02d %02d:%02d:%02d GMT",
        week[tm.tm_wday],
        tm.tm_mday,
        months[tm.tm_mon - 1],
        (tm.tm_year > 2037) ? tm.tm_year:
        tm.tm_year % 100,
        tm.tm_hour,
        tm.tm_min,
        tm.tm_sec);
}

void time_gmtime(time_t t, struct tm *tp)
{
    int      yday;
    uint32_t n, sec, min, hour, mday, mon, year, wday, days, leap;

    /* the calculation is valid for positive time_t only */

    n = (uint32_t) t;
    days = n / 86400;

    /* Jaunary 1, 1970 was Thursday */
    wday = (4 + days) % 7;
    n %= 86400;
    hour = n / 3600;
    n %= 3600;
    min = n / 60;
    sec = n % 60;
	
    /*
     * the algorithm based on Gauss' formula,
     * see src/http/dfs_http_parse_time.c
     */
    /* days since March 1, 1 BC */
    days = days - (31 + 28) + 719527;
	
    /*
     * The "days" should be adjusted to 1 only, however, some March 1st's go
     * to previous year, so we adjust them to 2.  This causes also shift of the
     * last Feburary days to next year, but we catch the case when "yday"
     * becomes negative.
     */
    year = (days + 2) * 400 / (365 * 400 + 100 - 4 + 1);
    yday = days - (365 * year + year / 4 - year / 100 + year / 400);
    if (yday < 0) 
	{
        leap = (year % 4 == 0) && (year % 100 || (year % 400 == 0));
        yday = 365 + leap + yday;
        year--;
    }
	
    /*
     * The empirical formula that maps "yday" to month.
     * There are at least 10 variants, some of them are:
     *     mon = (yday + 31) * 15 / 459
     *     mon = (yday + 31) * 17 / 520
     *     mon = (yday + 31) * 20 / 612
     */
    mon = (yday + 31) * 10 / 306;
    /* the Gauss' formula that evaluates days before the month */
    mday = yday - (367 * mon / 12 - 30) + 1;
    if (yday >= 306) 
	{
        year++;
        mon -= 10;
        /*
         * there is no "yday" in Win32 SYSTEMTIME
         *
         * yday -= 306;
         */
    } 
	else 
	{
        mon += 2;
        /*
         * there is no "yday" in Win32 SYSTEMTIME
         *
         * yday += 31 + 28 + leap;
         */
    }
	
    tp->tm_sec = (int) sec;
    tp->tm_min = (int) min;
    tp->tm_hour = (int) hour;
    tp->tm_mday = (int) mday;
    tp->tm_mon = (int) mon;
    tp->tm_year = (int) year;
    tp->tm_wday = (int) wday;
}

int time_monthtoi(const char *s)
{
    char ch1 = 0;
    char ch2 = 0;

    ch1 = *(s + 1);
    ch2 = *(s + 2);
	
    switch (*s) 
	{
    case 'J':
        if (ch1 == 'a') 
		{
            if (ch2 == 'n') 
			{
                return TIME_MONTH_JANUARY;
            }
			
            return DFS_ERROR;
        }
		
        if (ch1 != 'u') 
		{
            return DFS_ERROR;
        }
		
        if (ch2 == 'n') 
		{
            return TIME_MONTH_JUNE;
        }
		
        if (ch2 == 'l') 
		{
            return TIME_MONTH_JULY;
        }
        break;
		
    case 'F':
        if (ch1 == 'e' && ch2 == 'b') 
		{
            return TIME_MONTH_FEBRUARY;
        }
        break;
		
    case 'M':
        if (ch1 != 'a') 
		{
            return DFS_ERROR;
        }
		
        if (ch2 == 'r') 
		{
            return TIME_MONTH_MARCH;
        }
		
        if (ch2 == 'y') 
		{
            return TIME_MONTH_MAY;
        }
        break;
		
    case 'A':
        if (ch1 == 'p' && ch2 == 'r') 
		{
            return TIME_MONTH_APRIL;
        }
		
        if (ch1 == 'u' && ch2 == 'g') 
		{
            return TIME_MONTH_AUGUST;
        }
        break;
		
    case 'S':
        if (ch1 == 'e' && ch2 == 'p') 
		{
            return TIME_MONTH_SEPTEMBER;
        }
        break;
		
    case 'O':
        if (ch1 == 'c' && ch2 == 't') 
		{
            return TIME_MONTH_OCTOBER;
        }
        break;
		
    case 'N':
        if (ch1 == 'o' && ch2 == 'v') 
		{
            return TIME_MONTH_NOVEMBER;
        }
        break;
		
    case 'D':
        if (ch1 == 'e' && ch2 == 'c') 
		{
            return TIME_MONTH_DECEMBER;
        }
        break;
		
    default:
        break;
    }

    return DFS_ERROR;
}

