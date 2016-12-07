#include <stdlib.h>
#include <pthread.h>
#include <assert.h>

#include "nn_task_queue.h"

void queue_node_destory(task_queue_node_t *node, opq_free free_fn)
{
	assert(node != NULL);
	
	if (free_fn&& node->tk.opq)
	{
		free(node->tk.opq);
	}
	
	free(node);
}


void queue_node_clear(task_queue_node_t *node)
{
	void* opq = node->tk.opq;
	memset(&(node->tk),0x00,sizeof(task_t));
	queue_init(&(node->qe));
	node->tk.opq = opq;
}

void queue_node_set_task(task_queue_node_t *node, task_t *t)
{
	assert(node);
	assert(t);

	memcpy(&(node->tk), t, sizeof(task_t));
}

task_queue_node_t * queue_node_create()
{
	task_queue_node_t *t = (task_queue_node_t *)malloc(sizeof(task_queue_node_t));
	if (!t)
	{
		return NULL;
	}
	
	queue_node_clear(t);

	return t;
}

int task_queue_init(task_queue_t* tq)
{
	if (pthread_spin_init(&tq->lock, 0) != 0)
	{
		free(tq);
		
		return -1;
	}

	queue_init(&(tq->qh));
	
	return 0;
}

task_queue_t *task_queue_create()
{
	task_queue_t* tq = (task_queue_t*)malloc(sizeof(task_queue_t));
	if (tq == NULL)
	{
		return NULL;
	}
	
	queue_init(&tq->qh);
	
	if (pthread_spin_init(&tq->lock, 0) != 0)
	{
		free(tq);
		
		return NULL;
	}

	queue_init(&(tq->qh));
	
	return tq;
}

void task_queue_destory(task_queue_t* q)
{
	pthread_spin_destroy(&(q->lock));
	free(q);
}

task_queue_node_t* pop_task(task_queue_t* queue)
{
	task_queue_node_t *tnode = NULL;
	queue_t           *q = NULL;
	
	assert(queue);
	
	pthread_spin_lock(&queue->lock);
	
	if (queue_empty(&queue->qh))
	{
		pthread_spin_unlock(&queue->lock);
		
		return NULL;
	}

	q = queue_tail(&queue->qh);
	queue_remove(q);
	
	pthread_spin_unlock(&queue->lock);

	tnode = queue_data(q, task_queue_node_t, qe);

	return tnode;
}

void pop_all(task_queue_t* tq, queue_t* queue)
{
	pthread_spin_lock(&tq->lock);
	
	if (!queue_empty(&tq->qh)) 
	{
        queue->next = tq->qh.next;
        queue->prev = tq->qh.prev;
        
        queue->next->prev = queue;
        queue->prev->next = queue;
        queue_init(&tq->qh);
	}
	
	pthread_spin_unlock(&tq->lock);
}

void push_task(task_queue_t*queue, task_queue_node_t* tnode)
{
	assert(queue);
	assert(tnode);
	
	pthread_spin_lock(&queue->lock);
	
	queue_insert_head(&queue->qh, &tnode->qe);
	
	pthread_spin_unlock(&queue->lock);
}

