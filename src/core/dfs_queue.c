#include "dfs_queue.h"

/*
 * find the middle queue element if the queue has odd number of elements
 * or the first element of the queue's second part otherwise
 */
queue_t *queue_middle(queue_t *queue)
{
    queue_t *middle = NULL, *next = NULL;

    middle = queue_head(queue);
    if (middle == queue_tail(queue)) 
	{
        return middle;
    }
	
    next = queue_head(queue);
    for ( ;; ) 
	{
        middle = queue_next(middle);
        next = queue_next(next);
        if (next == queue_tail(queue)) 
		{
            return middle;
        }
		
        next = queue_next(next);
        if (next == queue_tail(queue)) 
		{
            return middle;
        }
    }
}

void queue_sort(queue_t *queue, int (*cmp)(const queue_t *, const queue_t *))
{
    queue_t *q = NULL, *prev = NULL, *next = NULL;

    q = queue_head(queue);
    if (q == queue_tail(queue)) 
	{
        return;
    }
	
    for (q = queue_next(q); q != queue_sentinel(queue); q = next) 
	{
        prev = queue_prev(q);
        next = queue_next(q);
		
        queue_remove(q);
		
        do 
		{
            if (cmp(prev, q) <= 0) 
			{
                break;
            }
			
            prev = queue_prev(prev);
        } while (prev != queue_sentinel(queue));
		
        queue_insert_head(prev, q);
    }
}

