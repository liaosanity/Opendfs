#ifndef NN_TASK_QUEUE_H
#define NN_TASK_QUEUE_H

#include <pthread.h>

#include "dfs_queue.h"
#include "dfs_task.h"

typedef struct task_queue_node_s
{
	task_t  tk;
	queue_t qe;
} task_queue_node_t;

typedef void (*opq_free)(void*);

typedef struct
{
	queue_t            qh;
	pthread_spinlock_t lock;
} task_queue_t;

task_queue_node_t * queue_node_create();
void queue_node_destory(task_queue_node_t *node, opq_free free_fn);
void queue_node_clear(task_queue_node_t *node);
void queue_node_set_task(task_queue_node_t *node, task_t *t);
int task_queue_init(task_queue_t* tq);
task_queue_t *task_queue_create();
void task_queue_destory(task_queue_t* q);
task_queue_node_t* pop_task(task_queue_t* queue);
void pop_all(task_queue_t*sq, queue_t* queue);
void push_task(task_queue_t*sq, task_queue_node_t* tnode);

#endif

