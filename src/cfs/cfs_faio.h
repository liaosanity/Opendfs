#ifndef CFS_FAIO_H
#define CFS_FAIO_H

#include "faio_manager.h"

void cfs_faio_setup(fs_meta_t *meta);
void cfs_faio_write_callback(faio_data_task_t *task);
void cfs_faio_read_callback(faio_data_task_t *task);
void cfs_faio_send_file_callback(faio_data_task_t *task);
int  cfs_faio_io_read(faio_data_task_t *task);
int  cfs_faio_io_write(faio_data_task_t *task);
int  cfs_faio_io_send_file(faio_data_task_t *task);

#endif

