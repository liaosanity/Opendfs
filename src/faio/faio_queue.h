#ifndef FAIO_QUEUE_H
#define FAIO_QUEUE_H

#include <stddef.h>

typedef struct faio_queue_s faio_queue_t;

struct faio_queue_s 
{
    faio_queue_t  *prev;
    faio_queue_t  *next;
};

#define faio_queue_init(q) \
    (q)->prev = q; \
    (q)->next = q

#define faio_queue_empty(h) \
    (h == (h)->prev)

#define faio_queue_sentinel(h) \
    (h)

#define faio_queue_head(h) \
    (h)->next

#define faio_queue_tail(h) \
    (h)->prev

#define faio_queue_next(q) \
    (q)->next

#define faio_queue_prev(q) \
    (q)->prev

#define faio_queue_data(q, type, link) \
    (type *) ((unsigned char *)q - offsetof(type, link))

#define faio_queue_insert_head(h, x) \
    (x)->next = (h)->next; \
    (x)->next->prev = x; \
    (x)->prev = h; \
    (h)->next = x

#define faio_queue_insert_tail(h, x) \
    (x)->prev = (h)->prev; \
    (x)->prev->next = x; \
    (x)->next = h; \
    (h)->prev = x

#define faio_queue_insert_before(listelm, elm)  \
do { \
    (elm)->prev = (listelm)->prev; \
    (elm)->next = (listelm); \
    (listelm)->prev->next = (elm); \
    (listelm)->prev = (elm); \
} while (0)

#define faio_queue_insert_after(listelm, elm) \
do { \
    (elm)->next = (listelm)->next; \
    (elm)->prev = (listelm); \
    (listelm)->next->prev = (elm); \
    (listelm)->next = (elm);\
} while (0)

#define faio_queue_add_queue(h, n) \
    (h)->prev->next = (n)->next; \
    (n)->next->prev = (h)->prev; \
    (h)->prev = (n)->prev; \
    (h)->prev->next = h;

#define faio_queue_remove(x) \
    (x)->next->prev = (x)->prev; \
    (x)->prev->next = (x)->next; \
    (x)->prev = (x)->next = NULL

#define faio_queue_split(h, q, n) \
    (n)->prev = (h)->prev; \
    (n)->prev->next = n; \
    (n)->next = q; \
    (h)->prev = (q)->prev; \
    (h)->prev->next = h; \
    (q)->prev = n;

#define  faio_queue_out(x)   !((x)->prev || (x)->next)
faio_queue_t *faio_queue_middle(faio_queue_t *queue);
void faio_queue_sort(faio_queue_t *queue,
    int (*cmp)(const faio_queue_t *, const faio_queue_t *));

#endif

