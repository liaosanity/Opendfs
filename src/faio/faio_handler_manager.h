#ifndef FAIO_HANDLER_MANAGER_H
#define FAIO_HANDLER_MANAGER_H

void faio_handler_manager_init(faio_handler_manager_t *handler_mgr);
void faio_handler_manager_release(faio_handler_manager_t *handler_mgr);
int faio_handler_register(faio_handler_manager_t *handler_mgr, 
    faio_io_handler_t handler, FAIO_IO_TYPE io_type, faio_errno_t *error);
void faio_handler_exec(faio_handler_manager_t *handler_mgr, 
    faio_data_task_t *task);

#endif

