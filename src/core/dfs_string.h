#ifndef DFS_STRING_H
#define DFS_STRING_H

#include "dfs_memory_pool.h"
#include "dfs_types.h"

struct string_s 
{
    size_t   len;
    uchar_t *data;
};

#define string_strskip_spaces(x)  while ((x) && (*(x) == ' ')) (x)++
#define string_strback_spaces(x)  while ((x) && (*(x) == ' ')) (x)--
#define string_strskip_char(x, y) while ((x) && (*(x) == (y))) (x)++
#define string_strback_char(x, y) while ((x) && (*(x) == (y))) (x)--

#define string_make(str)          { sizeof(str) - 1, (uchar_t *) (str) }
#define string_null               { 0, NULL }

#define string_strncmp(s1, s2, n) strncmp((const char *)(s1), (const char *)(s2), n)
#define string_strcmp(s1, s2)     strcmp((const char *)(s1), (const char *)(s2))
#define string_strcasecmp(s1, s2) strcasecmp((const char *)(s1), (const char *)(s2))
#define string_strncasecmp(s1, s2, n) strncasecmp((const char *)(s1), (const char *)(s2), n)
#define string_strstr(s1, s2)     strstr((const char *)(s1), (const char *)(s2))
//#define string_strchr(s1, c)      strchr((const char *)(s1), (int)(c))
#define string_strchr(s1, c)      strchr((char *)(s1), (int)(c))
#define string_strlen(s)          strlen((const char *)(s))
#define string_strncpy(s1, s2, n) strncpy((char *)(s1), (char *)(s2), (n))
#define string_isalnum(ch)        isalnum(ch)
#define string_strpbrk(s1, s2)    strpbrk((const char *)(s1), (const char *)(s2))
#define string_set(str, s)        str.data =(uchar_t *)s;\
                                  str.len = strlen((const char *)s)
#define string_ptr_set(str, s)    str->data = (uchar_t *)s;\
                                  str->len = sizeof(s) - 1
                                  
#define DFS_UNESCAPE_URI       1
#define DFS_UNESCAPE_REDIRECT  2

#define string_xxctolower(c) (uchar_t) ((((c) >= 'A') && ((c) <= 'Z')) ? ((c) | 0x20) : (c))
#define string_xxctoupper(c) (uchar_t) ((((c) >= 'a') && ((c) <= 'z')) ? ((c) & ~0x20) : (c))

void      string_strskip_chars(uchar_t **p, uchar_t *chars, size_t len);
uchar_t  *string_xxstrlchr(uchar_t *p, uchar_t *last, uchar_t c);
void      string_xxstrtolower(uchar_t *dst, uchar_t *src, size_t n);
char     *string_xxstrdup(const char *src);
int       string_xxstrcasecmp(uchar_t *s1, uchar_t *s2);
int       string_xxstrncasecmp(uchar_t *s1, uchar_t *s2, size_t n);
uchar_t  *string_xxstrnstr(uchar_t *s1, char *s2, size_t n);
uchar_t  *string_xxstrstrn(uchar_t *s1, char *s2, size_t n);
uchar_t  *string_xxstrcasestrn(uchar_t *s1, char *s2, size_t n);
uchar_t  *string_xxstrlcasestrn(uchar_t *s1, uchar_t *last,
    uchar_t *s2, size_t n);
uchar_t *string_xxstrlstrn(uchar_t *s1, uchar_t *last,
    uchar_t *s2, size_t n);
int       string_xxstrnrcmp(uchar_t *s1, uchar_t *s2, size_t n);
int       string_xxstrncasercmp(uchar_t *s1, uchar_t *s2, size_t n);
int       string_xxstrtoi(uchar_t *line, size_t n);
int       string_xxstrhextoi(uchar_t *line, size_t n);
ssize_t   string_xxstrtossize(uchar_t *line, size_t n);
uint32_t  string_xxstrtoui(uchar_t *line, size_t n);
size_t    string_xxstrtosize(uchar_t *line, size_t n);
time_t    string_xxstrtotime(uchar_t *line, size_t n);
uchar_t  *string_xxstrtohex(uchar_t *dst, uchar_t *src, size_t len);
uchar_t  *string_xxpdup(pool_t *pool, string_t *src);
uchar_t  *string_xxxpdup(pool_t *pool, uchar_t *src, size_t len);
uchar_t  *string_xxdup(string_t *src);
uchar_t  *_xcdecl string_xxsprintf(uchar_t *buf, const char *fmt, ...);
uchar_t  *_xcdecl string_xxsnprintf(uchar_t *buf,
    size_t max, const char *fmt, ...);
uchar_t  *string_xxvsnprintf(uchar_t *buf, size_t max,
    const char *fmt, va_list args);
void      string_base64_encode(string_t *dst, string_t *src);
int       string_base64_decode(string_t *dst, string_t *src);
uint32_t  string_utf8_decode(uchar_t **p, size_t n);
size_t    string_utf8_length(uchar_t *p, size_t n);
uchar_t  *string_utf8_cpystrn(uchar_t *dst, uchar_t *src, size_t n, 
	size_t len);
uintptr_t string_escape_uri(uchar_t *dst, uchar_t *src, size_t size,
	uint32_t type);
void      string_unescape_uri(uchar_t **dst, uchar_t **src, size_t size, 
	uint32_t type);
uintptr_t string_escape_html(uchar_t *dst, uchar_t *src, size_t size);
void      string_swap(string_t *str1, string_t *str2);

#endif

