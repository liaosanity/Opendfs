#include <assert.h>
#include <stdlib.h>

#include "nn_task_handler.h"
#include "nn_task_queue.h"
#include "dfs_queue.h"
#include "nn_thread.h"
#include "nn_rpc_server.h"

static void do_task(task_t *task)
{
    assert(task);
	nn_rpc_service_run(task);
}

void do_task_handler(void *q)
{
	task_queue_node_t *tnode = NULL;
	task_t            *t = NULL;
	queue_t           *cur = NULL;
	queue_t            qhead;
	task_queue_t      *tq = NULL;
    dfs_thread_t      *thread = NULL;

	tq = (task_queue_t *)q;
    thread = get_local_thread();
	
    queue_init(&qhead);
	pop_all(tq, &qhead);
	
	cur = queue_head(&qhead);
	
	while (!queue_empty(&qhead) && thread->running)
	{
		tnode = queue_data(cur, task_queue_node_t, qe);
		t = &tnode->tk;
		
        queue_remove(cur);

        do_task(t);
		
		cur = queue_head(&qhead);
	}
}

