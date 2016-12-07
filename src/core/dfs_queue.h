#ifndef DFS_QUEUE_H
#define DFS_QUEUE_H

#include "dfs_types.h"

typedef struct queue_s queue_t;

struct queue_s 
{
    queue_t *prev;
    queue_t *next;
};

#define queue_init(q) do {\
    (q)->prev = q; \
    (q)->next = q; \
} while (0)

#define queue_empty(h) \
    (h == (h)->prev)

#define queue_sentinel(h) \
    (h)

#define queue_head(h) \
    (h)->next

#define queue_tail(h) \
    (h)->prev

#define queue_next(q) \
    (q)->next

#define queue_prev(q) \
    (q)->prev
	
#define queue_data(q, type, link) \
    (type *) ((uchar_t *) q - offsetof(type, link))

#define queue_insert_head(h, x) \
    do {                       \
        (x)->next = (h)->next; \
        (x)->next->prev = x; \
        (x)->prev = h; \
        (h)->next = x; \
    } while (0)

#define queue_insert_tail(h, x) \
    do {                        \
        (x)->prev = (h)->prev; \
        (x)->prev->next = x; \
        (x)->next = h; \
        (h)->prev = x;  \
    } while (0)

#define queue_insert_before(listelm, elm)           \
    do {                                            \
        (elm)->prev = (listelm)->prev;              \
        (elm)->next = (listelm);                    \
        (listelm)->prev->next = (elm);              \
        (listelm)->prev = (elm);                    \
    } while (0)

#define queue_insert_after(listelm, elm)                    \
    do {                                                    \
        (elm)->next = (listelm)->next;                      \
        (elm)->prev = (listelm);                            \
        (listelm)->next->prev = (elm);                      \
        (listelm)->next = (elm);                            \
    } while (0)

#define queue_add_queue(h, n)        \
	do {                             \
        (h)->prev->next = (n)->next; \
        (n)->next->prev = (h)->prev; \
        (h)->prev = (n)->prev;       \
        (h)->prev->next = h;         \
	} while (0)

#define queue_remove(x)              \
    do {                             \
        (x)->next->prev = (x)->prev; \
        (x)->prev->next = (x)->next; \
        (x)->prev = (x)->next = NULL; \
	} while (0)
	
#define queue_split(h, q, n)   \
	do {                   \
        (n)->prev = (h)->prev; \
        (n)->prev->next = n; \
        (n)->next = q; \
        (h)->prev = (q)->prev; \
        (h)->prev->next = h; \
        (q)->prev = n;       \
	} while (0)

#define queue_for_each_entry(pos, head, member)                     \
        for (pos = queue_data((head)->next, typeof(*pos), member);  \
                         &pos->member != (head);                    \
         pos = queue_data(pos->member.next, typeof(*pos), member))
	
queue_t *queue_middle(queue_t *queue);
void queue_sort(queue_t *queue, int (*cmp)(const queue_t *, const queue_t *));

#endif

