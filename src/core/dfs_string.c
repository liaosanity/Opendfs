#include "dfs_string.h"
#include "dfs_memory.h"

void string_xxstrtolower(uchar_t *dst, uchar_t *src, size_t n)
{
    while (n) 
	{
        *dst = string_xxctolower(*src);
        dst++;
        src++;
        n--;
    }
}

void string_strskip_chars(uchar_t **p, uchar_t *chars, size_t len)
{
    uchar_t  ch = 0;
    uchar_t *pstart = NULL;
    uchar_t *pend = NULL;
    
    if (!p ||!(*p)) 
	{
        return;
    }
	
    pstart = chars;
    pend = chars + len;
    
    ch = **p;
    while (pstart < pend) 
	{
        if (ch == *pstart) 
		{
            (*p)++;
            ch = **p;
            pstart = chars;
			
            continue;
        }
		
        pstart++;
    }
}

uchar_t * string_xxstrlchr(uchar_t *p, uchar_t *last, uchar_t c)
{
    while (p < last) 
	{
        if (*p == c) 
		{
            return p;
        }
		
        p++;
    }

    return NULL;
}

uchar_t * string_xxpdup(pool_t *pool, string_t *src)
{
    uchar_t *dst = NULL;

    dst = (uchar_t *)pool_alloc(pool, src->len + 1);
    if (!dst) 
	{
        return NULL;
    }
	
    memory_memcpy(dst, src->data, src->len);
    dst[src->len] = 0;
    
    return dst;
}

uchar_t  *string_xxxpdup(pool_t *pool, uchar_t *src, size_t len)
{
    uchar_t *dst = NULL;

    dst = (uchar_t *)pool_alloc(pool, len + 1);
    if (!dst) 
	{
        return NULL;
    }
	
    memory_memcpy(dst, src, len);
    dst[len] = 0;
    
    return dst;
}

uchar_t * string_xxdup(string_t *src)
{
    uchar_t *dst = NULL;

    if (!src) 
	{
        return NULL;
    }
	
    dst = (uchar_t *)memory_alloc(src->len + 1);
    if (!dst) 
	{
        return NULL;
    }
	
    memory_memcpy(dst, src->data, src->len);
    dst[src->len] = 0;
    
    return dst;
}

int string_xxstrncasecmp(uchar_t *s1, uchar_t *s2, size_t n)
{
    uint32_t c1 = 0, c2 = 0;

    while (n) 
	{
        c1 = (uint32_t) *s1++;
        c2 = (uint32_t) *s2++;
        c1  = (c1 >= 'A' && c1 <= 'Z') ? (c1 | 0x20) : c1;
        c2  = (c2 >= 'A' && c2 <= 'Z') ? (c2 | 0x20) : c2;
		
        if (c1 == c2) 
		{
            if (c1) 
			{
                n--;
				
                continue;
            }
			
            return 0;
        }
		
        return c1 - c2;
    }
	
    return 0;
}

/*
 * supported formats:
 *    %[0][width][x][X]O        off_t
 *    %[0][width]T              time_t
 *    %[0][width][u][x|X]z      ssize_t/size_t
 *    %[0][width][u][x|X]d      int/uint32_t
 *    %[0][width][u][x|X]l      long
 *    %[0][width|m][u][x|X]i    int/uint32_t
 *    %[0][width][u][x|X]D      int32_t/uint32_t
 *    %[0][width][u][x|X]L      int64_t/uint64_t
 *    %[0][width|m][u][x|X]A    atomic_int_t/atomic_uint_t
 *    %P                        pid_t
 *    %M                        rb_msec_t
 *    %r                        rlim_t
 *    %p                        void *
 *    %V                        string_t *
 *    %s                        null-terminated string
 *    %*s                       length and string
 *    %Z                        '\0'
 *    %N                        '\n'
 *    %c                        char
 *    %%                        %
 *
 *  reserved:
 *    %t                        ptrdiff_t
 *    %S                        null-teminated wchar string
 *    %C                        wchar
 */
uchar_t * _xcdecl string_xxsprintf(uchar_t *buf, const char *fmt, ...)
{
    uchar_t *p = NULL;
    va_list  args;

    va_start(args, fmt);
    p = (uchar_t *)string_xxvsnprintf(buf, 65536, fmt, args);
    va_end(args);
	
    return p;
}

uchar_t * _xcdecl string_xxsnprintf(uchar_t *buf, size_t max, 
	                                     const char *fmt, ...)
{
    uchar_t *p = NULL;
    va_list  args;

    va_start(args, fmt);
    p = (uchar_t *)string_xxvsnprintf(buf, max, fmt, args);
    va_end(args);
	
    return p;
}

uchar_t * string_xxvsnprintf(uchar_t *buf, size_t max, 
	                              const char *fmt, va_list args)
{
    int             d = 0;
    uint32_t        width = 0;
    uint32_t        sign = 0;
    uint32_t        hexadecimal = 0;
    uint32_t        max_width = 0;
    size_t          len = 0;
    size_t          slen = 0;
    int64_t         i64 = 0;
    uint32_t        ui32 = 0;
    uint64_t        ui64 = 0;
    uchar_t        *p = NULL;
    uchar_t         zero = 0;
    uchar_t        *pend = NULL;
    uchar_t         temp[DFS_INT64_LEN + 1];
    rb_msec_t       ms;
    string_t       *v = NULL;
    static uchar_t  hex[] = "0123456789abcdef";
    static uchar_t  HEX[] = "0123456789ABCDEF";

    if (0 == max || !buf || !fmt) 
	{
        return buf;
    }
	
    pend = buf + max;
	
    while (*fmt && buf < pend) 
	{
        if (*fmt == '%') 
		{
            i64 = 0;
            ui64 = 0;
            zero = (uchar_t) ((*++fmt == '0') ? '0' : ' ');
            width = 0;
            sign = 1;
            hexadecimal = 0;
            max_width = 0;
            slen = (size_t) - 1;
            p = temp + DFS_INT64_LEN;
			
            while (*fmt >= '0' && *fmt <= '9') 
			{
                width = width * 10 + *fmt++ - '0';
            }
			
            for ( ;; ) 
			{
                switch (*fmt) 
				{
                case 'u':
                    sign = 0;
                    fmt++;
                    continue;
					
                case 'm':
                    max_width = 1;
                    fmt++;
                    continue;
					
                case 'X':
                    hexadecimal = 2;
                    sign = 0;
                    fmt++;
                    continue;
					
                case 'x':
                    hexadecimal = 1;
                    sign = 0;
                    fmt++;
                    continue;
					
                case '*':
                    slen = va_arg(args, size_t);
                    fmt++;
                    continue;
					
                default:
                    break;
                }
				
                break;
            }
			
            switch (*fmt) 
			{
            case 'V':
                v = va_arg(args, string_t *);
                len = v->len;
                len = (buf + len < pend) ? len : (size_t) (pend - buf);
                buf = memory_cpymem(buf, v->data, len);
                fmt++;
                continue;
				
            case 's':
                p = va_arg(args, uchar_t *);
                if (slen == (size_t) -1) 
				{
                    while (*p && buf < pend)
					{
                        *buf++ = *p++;
                    }
                } 
				else 
				{
                    len = (buf + slen < pend) ? slen : (size_t) (pend - buf);
                    buf = memory_cpymem(buf, p, len);
                }
				
                fmt++;
                continue;
				
            case 'O':
                i64 = (int64_t) va_arg(args, off_t);
                sign = 1;
                break;
				
            case 'P':
                i64 = (int64_t) va_arg(args, pid_t);
                sign = 1;
                break;
				
            case 'T':
                i64 = (int64_t) va_arg(args, time_t);
                sign = 1;
                break;
				
            case 'M':
                ms = (rb_msec_t) va_arg(args, rb_msec_t);
                if ((rb_msec_int_t) ms == -1) 
				{
                    sign = 1;
                    i64 = -1;
                } 
				else 
				{
                    sign = 0;
                    ui64 = (uint64_t) ms;
                }
                break;
				
            case 'z':
                if (sign) 
				{
                    i64 = (int64_t) va_arg(args, ssize_t);
                } 
				else 
				{
                    ui64 = (uint64_t) va_arg(args, size_t);
                }
                break;
				
            case 'i':
                if (sign) 
				{
                    i64 = (int64_t) va_arg(args, int);
                } 
				else 
				{
                    ui64 = (uint64_t) va_arg(args, uint32_t);
                }
				
                if (max_width) 
				{
                    width = DFS_INT_T_LEN;
                }
                break;
				
            case 'd':
                if (sign) 
				{
                    i64 = (int64_t) va_arg(args, int);
                } 
				else 
				{
                    ui64 = (uint64_t) va_arg(args, uint32_t);
                }
                break;
				
            case 'l':
                if (sign) 
				{
                    i64 = (int64_t) va_arg(args, long);
                } 
				else 
				{
                    ui64 = (uint64_t) va_arg(args, u_long);
                }
                break;
				
            case 'D':
                if (sign) 
				{
                    i64 = (int64_t) va_arg(args, int32_t);
                } 
				else 
				{
                    ui64 = (uint64_t) va_arg(args, uint32_t);
                }
                break;
				
            case 'L':
                if (sign) 
				{
                    i64 = va_arg(args, int64_t);
                } 
				else 
				{
                    ui64 = va_arg(args, uint64_t);
                }
                break;
				
            case 'A':
                if (sign) 
				{
                    i64 = (int64_t) va_arg(args, atomic_int_t);
                } 
				else 
				{
                    ui64 = (uint64_t) va_arg(args, atomic_uint_t);
                }
				
                if (max_width) 
				{
                    width = DFS_INT64_LEN;
                }
                break;
				
            case 'r':
                i64 = (int64_t) va_arg(args, rlim_t);
                sign = 1;
                break;
				
            case 'p':
                ui64 = (uintptr_t) va_arg(args, void *);
                hexadecimal = 2;
                sign = 0;
                zero = '0';
                width = DFS_PTR_SIZE * 2;
                break;
				
            case 'c':
                d = va_arg(args, int);
                *buf++ = (uchar_t) (d & 0xff);
                fmt++;
                continue;
				
            case 'Z':
                *buf++ = '\0';
                fmt++;
                continue;
				
            case 'N':
                *buf++ = LF;
                fmt++;
                continue;
				
            case '%':
                *buf++ = '%';
                fmt++;
                continue;
				
            default:
                *buf++ = *fmt++;
                continue;
            }
			
            if (sign) 
			{
                if (i64 < 0) 
				{
                    *buf++ = '-';
                    ui64 = (uint64_t) -i64;
                } 
				else 
				{
                    ui64 = (uint64_t) i64;
                }
            }
			
            if (1 == hexadecimal) 
			{
                do 
				{
                    *--p = hex[(uint32_t) (ui64 & 0xf)];
                } while (ui64 >>= 4);
            } 
			else if (2 == hexadecimal) 
			{
                do 
				{
                    *--p = HEX[(uint32_t) (ui64 & 0xf)];
                } while (ui64 >>= 4);
            } 
			else if (ui64 <= DFS_MAX_UINT32_VALUE) 
			{
                ui32 = (uint32_t) ui64;
                do 
				{
                    *--p = (uchar_t) (ui32 % 10 + '0');
                } while (ui32 /= 10);
            } 
			else 
			{
                do 
				{
                    *--p = (uchar_t) (ui64 % 10 + '0');
                } while (ui64 /= 10);
            }
			
            len = (temp + DFS_INT64_LEN) - p;
            while (len++ < width && buf < pend) 
			{
                *buf++ = zero;
            }
			
            len = (temp + DFS_INT64_LEN) - p;
            if (buf + len > pend) 
			{
                len = pend - buf;
            }
			
            buf = memory_cpymem(buf, p, len);
            fmt++;
        }
		else 
		{
            *buf++ = *fmt++;
        }
    }
	
    return buf;
}

/*
 * We use string_xxstrcasecmp()/string_xxstrncasecmp() for 7-bit ASCII strings only,
 * and implement our own string_xxstrcasecmp()/string_xxstrncasecmp()
 * to avoid libc locale overhead.  Besides, we use the uint32_t's
 * instead of the uchar_t's, because they are slightly dfser.
 */
int string_xxstrcasecmp(uchar_t *s1, uchar_t *s2)
{
    uint32_t c1 = 0, c2 = 0;

    for ( ;; )
	{
        c1 = (uint32_t) *s1++;
        c2 = (uint32_t) *s2++;
        c1  = (c1 >= 'A' && c1 <= 'Z') ? (c1 | 0x20) : c1;
        c2  = (c2 >= 'A' && c2 <= 'Z') ? (c2 | 0x20) : c2;
		
        if (c1 == c2) 
		{
            if (c1) 
			{
                continue;
            }
			
            return 0;
        }
		
        return c1 - c2;
    }
}

uchar_t * string_xxstrnstr(uchar_t *s1, char *s2, size_t len)
{
    uchar_t c1 = 0, c2 = 0;
    size_t  n = 0;

    c2 = *(uchar_t *) s2++;
    n = string_strlen(s2);
	
    do 
	{
        do 
		{
            if (0 == len--) 
			{
                return NULL;
            }
			
            c1 = *s1++;
			
            if (0 == c1) 
			{
                return NULL;
            }
        } while (c1 != c2);
		
        if (n > len) 
		{
            return NULL;
        }
    } while (string_strncmp(s1, (uchar_t *) s2, n) != 0);
	
    return --s1;
}

/*
 * string_xxstrstrn() and string_xxstrcasestrn() are intended to search for static
 * substring with known length in null-terminated string. The argument n
 * must be length of the second substring - 1.
 */
uchar_t * string_xxstrstrn(uchar_t *s1, char *s2, size_t n)
{
    uchar_t c1 = 0, c2 = 0;

    c2 = *(uchar_t *) s2++;
	
    do 
	{
        do 
		{
            c1 = *s1++;
			
            if (0 == c1) 
			{
                return NULL;
            }
        } while (c1 != c2);
    } while (string_strncmp(s1, (uchar_t *) s2, n) != 0);
	
    return --s1;
}

char * string_xxstrdup(const char *src)
{
    int   size = 0;
    char *dst = NULL;
    
    if (!src || src[0] == 0) 
	{
        return NULL;
    }
	
    size = string_strlen(src) + 1;
    dst = (char *)memory_alloc(size);
    if (!dst) 
	{
        return NULL;
    }
	
    if (!string_strncpy(dst, src, size)) 
	{
        memory_free(dst, size);
		
        return NULL;
    }
	
    dst[size - 1] = 0;
    
    return dst;
}

uchar_t * string_xxstrcasestrn(uchar_t *s1, char *s2, size_t n)
{
    uint32_t c1 = 0, c2 = 0;

    c2 = (uint32_t) *s2++;
    c2  = (c2 >= 'A' && c2 <= 'Z') ? (c2 | 0x20) : c2;
	
    do 
	{
        do 
		{
            c1 = (uint32_t) *s1++;
            if (0 == c1) 
			{
                return NULL;
            }
			
            c1  = (c1 >= 'A' && c1 <= 'Z') ? (c1 | 0x20) : c1;
        } while (c1 != c2);
    } while (string_xxstrncasecmp(s1, (uchar_t *) s2, n - 1) != 0);
	
    return --s1;
}

/*
 * ngx_strlcasestrn() is intended to search for static substring
 * with known length in string until the argument last. The argument n
 * must be length of the second substring - 1.
 */
uchar_t * string_xxstrlcasestrn(uchar_t *s1, uchar_t *last, 
                                     uchar_t *s2, size_t n)
{
    uint32_t c1 = 0, c2 = 0;

    c2 = (uint32_t) *s2++;
    c2  = (c2 >= 'A' && c2 <= 'Z') ? (c2 | 0x20) : c2;
    last -= n;

    do 
	{
        do 
		{
            if (s1 >= last) 
			{
                return NULL;
            }

            c1 = (uint32_t) *s1++;
            c1  = (c1 >= 'A' && c1 <= 'Z') ? (c1 | 0x20) : c1;
        } while (c1 != c2);

    } while (string_xxstrncasecmp(s1, s2, n) != 0);

    return --s1;
}

uchar_t * string_xxstrlstrn(uchar_t *s1, uchar_t *last, 
	                            uchar_t *s2, size_t n)
{
    uint32_t c1 = 0, c2 = 0;

    c2 = (uint32_t) *s2++;
    c2  = (c2 >= 'A' && c2 <= 'Z') ? (c2 | 0x20) : c2;
    last -= n;

    do 
	{
        do 
		{
            if (s1 >= last) 
			{
                return NULL;
            }

            c1 = (uint32_t) *s1++;
            c1  = (c1 >= 'A' && c1 <= 'Z') ? (c1 | 0x20) : c1;
        } while (c1 != c2);
    } while (string_strncmp(s1, s2, n) != 0);

    return --s1;
}

int string_xxstrnrcmp(uchar_t *s1, uchar_t *s2, size_t n)
{
    if (0 == n) 
	{
        return 0;
    }
	
    n--;
	
    for ( ;; ) 
	{
        if (s1[n] != s2[n]) 
		{
            return s1[n] - s2[n];
        }
		
        if (0 == n) 
		{
            return 0;
        }
		
        n--;
    }
}

int string_xxstrncasercmp(uchar_t *s1, uchar_t *s2, size_t n)
{
    uchar_t c1 = 0, c2 = 0;

    if (0 == n) 
	{
        return 0;
    }
	
    n--;
	
    for ( ;; ) 
	{
        c1 = s1[n];
        if (c1 >= 'a' && c1 <= 'z') 
		{
            c1 -= 'a' - 'A';
        }
		
        c2 = s2[n];
        if (c2 >= 'a' && c2 <= 'z') 
		{
            c2 -= 'a' - 'A';
        }
		
        if (c1 != c2) 
		{
            return c1 - c2;
        }
		
        if (0 == n) 
		{
            return 0;
        }
		
        n--;
    }
}

int string_xxstrtoi(uchar_t *line, size_t n)
{
    int negflag = DFS_FALSE;
    int value = 0;

    if (0 == n) 
	{
        return 0;
    }
	
    if (*line == '-') 
	{
        negflag = DFS_TRUE;
        line++;
    }
	
    for (value = 0; n--; line++) 
	{
        if (*line < '0' || *line > '9') 
		{
            return negflag ? -value : value;
        }
		
        value = value * 10 + (*line - '0');
    }
    
    return negflag ? -value : value;
}

int string_xxstrhextoi(uchar_t *line, size_t n)
{
    int     negflag = DFS_FALSE;
    int     value = 0;
    uchar_t c = 0, ch = 0;

    if (0 == n) 
	{
        return 0;
    }
	
    if (*line == '-') 
	{
        negflag = DFS_TRUE;
        line++;
    }
	
    for (value = 0; n--; line++) 
	{
        ch = *line;
        if (ch >= '0' && ch <= '9') 
		{
            value = value * 16 + (ch - '0');
			
            continue;
        }
		
        c = (uchar_t) (ch | 0x20);
        if (c >= 'a' && c <= 'f') 
		{
            value = value * 16 + (c - 'a' + 10);
			
            continue;
        }
		
        return negflag ? -value : value;
    }

    return negflag ? -value : value;
}

ssize_t string_xxstrtossize(uchar_t *line, size_t n)
{
    int     negflag = DFS_FALSE;
    ssize_t value = 0;

    if (0 == n) 
	{
        return 0;
    }
	
    if (*line == '-') 
	{
        negflag = DFS_TRUE;
        line++;
    }
	
    for (value = 0; n--; line++) 
	{
        if (*line < '0' || *line > '9') 
		{
            return negflag ? -value : value;
        }
		
        value = value * 10 + (*line - '0');
    }
    
    return negflag ? -value: value;
}

uint32_t string_xxstrtoui(uchar_t *line, size_t n)
{
    uint32_t value = 0;

    if (0 == n) 
	{
        return DFS_ERROR;
    }
	
    for (value = 0; n--; line++) 
	{
        if (*line < '0' || *line > '9') 
		{
            return DFS_ERROR;
        }
		
        value = value * 10 + (*line - '0');
    }
    
    return value;
}

size_t string_xxstrtosize(uchar_t *line, size_t n)
{
    size_t value = 0;

    if (0 == n) 
	{
        return DFS_ERROR;
    }
	
    for (value = 0; n--; line++) 
	{
        if (*line < '0' || *line > '9') 
		{
            return DFS_ERROR;
        }
		
        value = value * 10 + (*line - '0');
    }
    
    return value;
}

time_t string_xxstrtotime(uchar_t *line, size_t n)
{
    time_t value;

    if (0 == n) 
	{
        return DFS_ERROR;
    }
	
    for (value = 0; n--; line++) 
	{
        if (*line < '0' || *line > '9') 
		{
            return DFS_ERROR;
        }
		
        value = value * 10 + (*line - '0');
    }
    
    return value;
}

uchar_t * string_xxstrtohex(uchar_t *dst, uchar_t *src, size_t len)
{
    static uchar_t hex[] = "0123456789abcdef";

    while (len--) 
	{
        *dst++ = hex[*src >> 4];
        *dst++ = hex[*src++ & 0xf];
    }
    
    return dst;
}

void string_base64_encode(string_t *dst, string_t *src)
{
    size_t          len = 0;
    uchar_t        *d = NULL, *s = NULL;
    static uchar_t  basis64[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    len = src->len;
    s = src->data;
    d = dst->data;

    while (len > 2) 
	{
        *d++ = basis64[(s[0] >> 2) & 0x3f];
        *d++ = basis64[((s[0] & 3) << 4) | (s[1] >> 4)];
        *d++ = basis64[((s[1] & 0x0f) << 2) | (s[2] >> 6)];
        *d++ = basis64[s[2] & 0x3f];
        s += 3;
        len -= 3;
    }
	
    if (len) 
	{
        *d++ = basis64[(s[0] >> 2) & 0x3f];
		
        if (len == 1) 
		{
            *d++ = basis64[(s[0] & 3) << 4];
            *d++ = '=';
        } 
		else 
		{
            *d++ = basis64[((s[0] & 3) << 4) | (s[1] >> 4)];
            *d++ = basis64[(s[1] & 0x0f) << 2];
        }
		
        *d++ = '=';
    }
	
    dst->len = d - dst->data;
}

int string_base64_decode(string_t *dst, string_t *src)
{
    size_t          len = 0;
    uchar_t        *d = NULL, *s = NULL;
    static uchar_t  basis64[] = 
	{
        77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77,
        77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77,
        77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 62, 77, 77, 77, 63,
        52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 77, 77, 77, 77, 77, 77,
        77,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14,
        15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 77, 77, 77, 77, 77,
        77, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
        41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 77, 77, 77, 77, 77,

        77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77,
        77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77,
        77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77,
        77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77,
        77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77,
        77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77,
        77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77,
        77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77
    };

    for (len = 0; len < src->len; len++) 
	{
        if ('=' == src->data[len]) 
		{
            break;
        }
		
        if (basis64[src->data[len]] == 77) 
		{
            return DFS_ERROR;
        }
    }
	
    if (1 == (len % 4)) 
	{
        return DFS_ERROR;
    }
	
    s = src->data;
    d = dst->data;
	
    while (len > 3) 
	{
        *d++ = (uchar_t) (basis64[s[0]] << 2 | basis64[s[1]] >> 4);
        *d++ = (uchar_t) (basis64[s[1]] << 4 | basis64[s[2]] >> 2);
        *d++ = (uchar_t) (basis64[s[2]] << 6 | basis64[s[3]]);
        s += 4;
        len -= 4;
    }
	
    if (len > 1) 
	{
        *d++ = (uchar_t) (basis64[s[0]] << 2 | basis64[s[1]] >> 4);
    }
	
    if (len > 2) 
	{
        *d++ = (uchar_t) (basis64[s[1]] << 4 | basis64[s[2]] >> 2);
    }
	
    dst->len = d - dst->data;
	
    return DFS_OK;
}

/*
 * string_utf8_decode() decodes two and more bytes UTF sequences only
 * the return values:
 *    0x80 - 0x10ffff         valid character
 *    0x110000 - 0xfffffffd   invalid sequence
 *    0xfffffffe              incomplete sequence
 *    0xffffffff              error
 */
uint32_t string_utf8_decode(uchar_t **p, size_t n)
{
    size_t   len = 0;
    uint32_t u = 0, i = 0, valid = 0;

    u = **p;

    if (u > 0xf0)
	{
        u &= 0x07;
        valid = 0xffff;
        len = 3;
    } 
	else if (u > 0xe0) 
	{
        u &= 0x0f;
        valid = 0x7ff;
        len = 2;
    } 
	else if (u > 0xc0) 
	{
        u &= 0x1f;
        valid = 0x7f;
        len = 1;
    } 
	else 
	{
        (*p)++;
        return 0xffffffff;
    }
	
    if (n - 1 < len) 
	{
        return 0xfffffffe;
    }
	
    (*p)++;
	
    while (len) 
	{
        i = *(*p)++;
        if (i < 0x80) 
		{
            return 0xffffffff;
        }
		
        u = (u << 6) | (i & 0x3f);
        len--;
    }
	
    if (u > valid) 
	{
        return u;
    }
	
    return 0xffffffff;
}

size_t string_utf8_length(uchar_t *p, size_t n)
{
    uchar_t c = 0, *last = NULL;
    size_t  len = 0;

    last = p + n;
	
    for (len = 0; p < last; len++) 
	{
        c = *p;
        if (c < 0x80) 
		{
            p++;
			
            continue;
        }
		
        if (string_utf8_decode(&p, n) > 0x10ffff) 
		{
            // invalid UTF-8
            return n;
        }
    }
	
    return len;
}

uchar_t * string_utf8_cpystrn(uchar_t *dst, uchar_t *src, 
	                                size_t n, size_t len)
{
    uchar_t c = 0, *next = NULL;

    if (0 == n) 
	{
        return dst;
    }
	
    while (--n) 
	{
        c = *src;
        *dst = c;
        if (c < 0x80) 
		{
            if (c != '\0') 
			{
                dst++;
                src++;
                len--;
				
                continue;
            }
			
            return dst;
        }
		
        next = src;
        if (string_utf8_decode(&next, len) > 0x10ffff) 
		{
            // invalid UTF-8
            break;
        }
		
        len--;
        while (src < next) 
		{
            *++dst = *++src;
            len--;
        }
    }
	
    *dst = '\0';
	
    return dst;
}

uintptr_t string_escape_uri(uchar_t *dst, uchar_t *src, 
	                             size_t size, uint32_t type)
{
    uint32_t        i = 0, n = 0;
    uint32_t       *escape = NULL;
    static uchar_t  hex[] = "0123456789abcdef";

    /* " ", "#", "%", "?", %00-%1F, %7F-%FF */
    static uint32_t uri[] = 
    {
        0xffffffff, /* 1111 1111 1111 1111  1111 1111 1111 1111 */

                    /* ?>=< ;:98 7654 3210  /.-, +*)( '&%$ #"!  */
        0x80000029, /* 1000 0000 0000 0000  0000 0000 0010 1001 */

                    /* _^]\ [ZYX WVUT SRQP  ONML KJIH GFED CBA@ */
        0x00000000, /* 0000 0000 0000 0000  0000 0000 0000 0000 */

                    /*  ~}| {zyx wvut srqp  onml kjih gfed cba` */
        0x80000000, /* 1000 0000 0000 0000  0000 0000 0000 0000 */

        0xffffffff, /* 1111 1111 1111 1111  1111 1111 1111 1111 */
        0xffffffff, /* 1111 1111 1111 1111  1111 1111 1111 1111 */
        0xffffffff, /* 1111 1111 1111 1111  1111 1111 1111 1111 */
        0xffffffff  /* 1111 1111 1111 1111  1111 1111 1111 1111 */
    };

    /* " ", "#", "%", "+", "?", %00-%1F, %7F-%FF */
    static uint32_t args[] = 
    {
        0xffffffff, /* 1111 1111 1111 1111  1111 1111 1111 1111 */

                    /* ?>=< ;:98 7654 3210  /.-, +*)( '&%$ #"!  */
        0x80000829, /* 1000 0000 0000 0000  0000 1000 0010 1001 */

                    /* _^]\ [ZYX WVUT SRQP  ONML KJIH GFED CBA@ */
        0x00000000, /* 0000 0000 0000 0000  0000 0000 0000 0000 */

                    /*  ~}| {zyx wvut srqp  onml kjih gfed cba` */
        0x80000000, /* 1000 0000 0000 0000  0000 0000 0000 0000 */

        0xffffffff, /* 1111 1111 1111 1111  1111 1111 1111 1111 */
        0xffffffff, /* 1111 1111 1111 1111  1111 1111 1111 1111 */
        0xffffffff, /* 1111 1111 1111 1111  1111 1111 1111 1111 */
        0xffffffff  /* 1111 1111 1111 1111  1111 1111 1111 1111 */
    };

    /* " ", "#", """, "%", "'", %00-%1F, %7F-%FF */
    static uint32_t html[] = 
    {
        0xffffffff, /* 1111 1111 1111 1111  1111 1111 1111 1111 */

                    /* ?>=< ;:98 7654 3210  /.-, +*)( '&%$ #"!  */
        0x000000ad, /* 0000 0000 0000 0000  0000 0000 1010 1101 */

                    /* _^]\ [ZYX WVUT SRQP  ONML KJIH GFED CBA@ */
        0x00000000, /* 0000 0000 0000 0000  0000 0000 0000 0000 */

                    /*  ~}| {zyx wvut srqp  onml kjih gfed cba` */
        0x80000000, /* 1000 0000 0000 0000  0000 0000 0000 0000 */

        0xffffffff, /* 1111 1111 1111 1111  1111 1111 1111 1111 */
        0xffffffff, /* 1111 1111 1111 1111  1111 1111 1111 1111 */
        0xffffffff, /* 1111 1111 1111 1111  1111 1111 1111 1111 */
        0xffffffff  /* 1111 1111 1111 1111  1111 1111 1111 1111 */
    };

    /* " ", """, "%", "'", %00-%1F, %7F-%FF */
    static uint32_t refresh[] = 
    {
        0xffffffff, /* 1111 1111 1111 1111  1111 1111 1111 1111 */

                    /* ?>=< ;:98 7654 3210  /.-, +*)( '&%$ #"!  */
        0x00000085, /* 0000 0000 0000 0000  0000 0000 1000 0101 */

                    /* _^]\ [ZYX WVUT SRQP  ONML KJIH GFED CBA@ */
        0x00000000, /* 0000 0000 0000 0000  0000 0000 0000 0000 */

                    /*  ~}| {zyx wvut srqp  onml kjih gfed cba` */
        0x80000000, /* 1000 0000 0000 0000  0000 0000 0000 0000 */

        0xffffffff, /* 1111 1111 1111 1111  1111 1111 1111 1111 */
        0xffffffff, /* 1111 1111 1111 1111  1111 1111 1111 1111 */
        0xffffffff, /* 1111 1111 1111 1111  1111 1111 1111 1111 */
        0xffffffff  /* 1111 1111 1111 1111  1111 1111 1111 1111 */
    };

    /* " ", "%", %00-%1F */
    static uint32_t memcached[] = 
    {
        0xffffffff, /* 1111 1111 1111 1111  1111 1111 1111 1111 */

                    /* ?>=< ;:98 7654 3210  /.-, +*)( '&%$ #"!  */
        0x00000021, /* 0000 0000 0000 0000  0000 0000 0010 0001 */

                    /* _^]\ [ZYX WVUT SRQP  ONML KJIH GFED CBA@ */
        0x00000000, /* 0000 0000 0000 0000  0000 0000 0000 0000 */

                    /*  ~}| {zyx wvut srqp  onml kjih gfed cba` */
        0x00000000, /* 0000 0000 0000 0000  0000 0000 0000 0000 */

        0x00000000, /* 0000 0000 0000 0000  0000 0000 0000 0000 */
        0x00000000, /* 0000 0000 0000 0000  0000 0000 0000 0000 */
        0x00000000, /* 0000 0000 0000 0000  0000 0000 0000 0000 */
        0x00000000, /* 0000 0000 0000 0000  0000 0000 0000 0000 */
    };

    /* mail_auth is the same as memcached */
    static uint32_t *map[] =
        { uri, args, html, refresh, memcached, memcached };

    escape = map[type];
    if (NULL == dst) 
	{
        /* find the number of the characters to be escaped */
        n  = 0;
        for (i = 0; i < size; i++) 
		{
            if (escape[*src >> 5] & (1 << (*src & 0x1f))) 
			{
                n++;
            }
            src++;
        }
		
        return (uintptr_t) n;
    }
	
    for (i = 0; i < size; i++) 
	{
        if (escape[*src >> 5] & (1 << (*src & 0x1f))) 
		{
            *dst++ = '%';
            *dst++ = hex[*src >> 4];
            *dst++ = hex[*src & 0xf];
            src++;
        } 
		else 
		{
            *dst++ = *src++;
        }
    }
	
    return (uintptr_t) dst;
}

void string_unescape_uri(uchar_t **dst, uchar_t **src, 
	                            size_t size, uint32_t type)
{
    uchar_t *d = NULL, *s = NULL, ch = 0, c = 0, decoded = 0;
	
    enum 
	{
        sw_usual = 0,
        sw_quoted,
        sw_quoted_second
    } state;

    d = *dst;
    s = *src;
    //state = 0;
    state = sw_usual;
    decoded = 0;

    while (size--) 
	{
        ch = *s++;
		
        switch (state) 
		{
        case sw_usual:
            if (ch == '?'
                && (type & (DFS_UNESCAPE_URI|DFS_UNESCAPE_REDIRECT))) 
            {
                *d++ = ch;
				
                goto done;
            }
			
            if (ch == '%') 
		    {
                state = sw_quoted;
				
                break;
            }
			
            *d++ = ch;
            break;
			
        case sw_quoted:
            if (ch >= '0' && ch <= '9') 
			{
                decoded = (uchar_t) (ch - '0');
                state = sw_quoted_second;
				
                break;
            }
			
            c = (uchar_t) (ch | 0x20);
            if (c >= 'a' && c <= 'f') 
			{
                decoded = (uchar_t) (c - 'a' + 10);
                state = sw_quoted_second;
				
                break;
            }

            state = sw_usual;
            *d++ = ch;
            break;
			
        case sw_quoted_second:
            state = sw_usual;
            if (ch >= '0' && ch <= '9') 
			{
                ch = (uchar_t) ((decoded << 4) + ch - '0');
                if (type & DFS_UNESCAPE_REDIRECT) 
				{
                    if (ch > '%' && ch < 0x7f) 
					{
                        *d++ = ch;
						
                        break;
                    }
					
                    *d++ = '%'; *d++ = *(s - 2); *d++ = *(s - 1);
                    break;
                }
				
                *d++ = ch;
				
                break;
            }
			
            c = (uchar_t) (ch | 0x20);
            if (c >= 'a' && c <= 'f') 
			{
                ch = (uchar_t) ((decoded << 4) + c - 'a' + 10);
                if (type & DFS_UNESCAPE_URI) 
				{
                    if (ch == '?') 
					{
                        *d++ = ch;
						
                        goto done;
                    }
					
                    *d++ = ch;
					
                    break;
                }
				
                if (type & DFS_UNESCAPE_REDIRECT) 
				{
                    if (ch == '?') 
					{
                        *d++ = ch;
						
                        goto done;
                    }
					
                    if (ch > '%' && ch < 0x7f) 
					{
                        *d++ = ch;
						
                        break;
                    }
					
                    *d++ = '%'; *d++ = *(s - 2); *d++ = *(s - 1);
					
                    break;
                }
				
                *d++ = ch;
                break;
            }

            break;
        }
    }
	
done:
    *dst = d;
    *src = s;
}

uintptr_t string_escape_html(uchar_t *dst, uchar_t *src, size_t size)
{
    uchar_t  ch = 0;
    uint32_t i = 0, len = 0;

    if (NULL == dst) 
	{
        len = 0;

        for (i = 0; i < size; i++) 
		{
            switch (*src++) 
			{
            case '<':
                len += sizeof("&lt;") - 2;
                break;
				
            case '>':
                len += sizeof("&gt;") - 2;
                break;
				
            case '&':
                len += sizeof("&amp;") - 2;
                break;
				
            default:
                break;
            }
        }
		
        return (uintptr_t) len;
    }
	
    for (i = 0; i < size; i++) 
	{
        ch = *src++;

        switch (ch) 
		{
        case '<':
            *dst++ = '&'; *dst++ = 'l'; *dst++ = 't'; *dst++ = ';';
            break;
			
        case '>':
            *dst++ = '&'; *dst++ = 'g'; *dst++ = 't'; *dst++ = ';';
            break;
			
        case '&':
            *dst++ = '&'; *dst++ = 'a'; *dst++ = 'm'; *dst++ = 'p';
            *dst++ = ';';
            break;
			
        default:
            *dst++ = ch;
            break;
        }
    }
	
    return (uintptr_t) dst;
}

void string_swap(string_t *str1, string_t *str2)
{
    string_t tmp;
    tmp = *str1;
    *str1 = *str2;
    *str2 = tmp;
}

