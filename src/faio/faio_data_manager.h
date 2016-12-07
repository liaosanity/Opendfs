#ifndef FAIO_DATA_MANAGER_H
#define FAIO_DATA_MANAGER_H

int faio_data_manager_init(faio_manager_t *faio_mgr, 
    unsigned int max_task, faio_errno_t *error);
int faio_data_manager_release(faio_data_manager_t *data_mgr, 
    faio_errno_t *error);
faio_data_task_t *faio_data_pop_req(faio_data_manager_t *data_mgr);
size_t faio_data_manager_get_size(faio_data_manager_t *data_mgr);
int faio_data_push_task(faio_data_manager_t *data_mgr, 
	faio_data_task_t *task, faio_notifier_manager_t *notifier, 
	faio_callback_t io_callback, FAIO_IO_TYPE io_type, faio_errno_t *error);

#endif
