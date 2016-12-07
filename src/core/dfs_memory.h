#ifndef DFS_MEMORY_H
#define DFS_MEMORY_H

#include "dfs_types.h"

/*
 * msvc and icc7 compile memset() to the inline "rep stos"
 * while ZeroMemory() and bzero() are the calls.
 * icc7 may also inline several mov's of a zeroed register for small blocks.
 */
#define memory_zero(buf, n)        (void) memset(buf, 0, n)
#define memory_realloc(ptr, size)  realloc(ptr, size)

/*
 * gcc3, msvc, and icc7 compile memcpy() to the inline "rep movs".
 * gcc3 compiles memcpy(d, s, 4) to the inline "mov"es.
 * icc8 compile memcpy(d, s, 4) to the inline "mov"es or XMM moves.
 */
#define memory_memcpy(dst, src, n) (void) memcpy(dst, src, n)
#define memory_cpymem(dst, src, n) ((uchar_t *) memcpy(dst, src, n)) + (n)
/* msvc and icc7 compile memcmp() to the inline loop */
#define memory_memcmp(s1, s2, n)   memcmp((const char *) s1, (const char *) s2, n)

void *memory_alloc(size_t size);
void *memory_calloc(size_t size);
void  memory_free(void *p, size_t size);
void *memory_memalign(size_t alignment, size_t size);
int   memory_n2cmp(uchar_t *s1, uchar_t *s2, size_t n1, size_t n2);

#endif

