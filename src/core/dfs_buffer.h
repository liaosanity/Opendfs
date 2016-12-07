#ifndef DFS_BUF_H
#define DFS_BUF_H

#include "dfs_types.h"

struct buffer_s 
{
    uchar_t  *pos;         // write out position
    uchar_t  *last;        // read in position
    off_t     file_pos;    // write out position
    off_t     file_last;   //
    uchar_t  *start;       // start of buffer
    uchar_t  *end;         // end of buffer
    uint32_t  temporary:1; // alloc in pool, need not free
    uint32_t  memory:1;    // memory, not file
    uint32_t  in_file:1;   // file flag
};

typedef struct chunk_s chunk_t;

struct chunk_s
{
    buffer_t *hdr;  // 64-bit hexadimal string
    ssize_t   size;
    chunk_t  *next;
};

#define buffer_in_memory(b)      ((b)->memory)// || (b)->mmap || (b)->memory
#define buffer_in_memory_only(b) (buffer_in_memory((b)) && !(b)->in_file)
#define buffer_size(b) \
    (buffer_in_memory(b) ? ((b)->last - (b)->pos): \
    ((b)->file_last - (b)->file_pos))
#define buffer_free_size(b) ((b)->end - (b)->last)
#define buffer_full(b)      ((b)->end == (b)->pos)
#define buffer_reset(b)     ((b)->last = (b)->pos = (b)->start)
#define buffer_pull(b,s)    ((b)->pos += (s))
#define buffer_used(b,s)    ((b)->last += (s))

buffer_t *buffer_create(pool_t *pool, size_t size);
buffer_t *buffer_alloc(pool_t *p);
void      buffer_free(buffer_t *buf);
void      buffer_shrink(buffer_t* buf);

#endif

