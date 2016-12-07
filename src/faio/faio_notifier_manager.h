#ifndef FAIO_NOTIFIER_H
#define FAIO_NOTIFIER_H

#include "faio_error.h"
#include "faio_manager.h"

#define FAIO_NOTIFIER_DEFAULT_TIMEOUT (500) // 500ms

#define faio_notifier_get_nfd(n) ((n)->nfd)
#define faio_notifier_get_manager(n) ((n)->manager)

#if (__GNUC__ >= 4 && __GNUC_MINOR__ >=2 )
#define CAS(val, old, set) \
    __sync_bool_compare_and_swap((val), (old), (set))
#else
#define CAS(val, old, set) \
        cmp_and_swap((val), (old), (set))
#endif

#define faio_notifier_count_equal(x, y) (CAS(&(x)->value, (y), (y)))
#define faio_notifier_count_reset(x) ((x)->value = 0)
#define faio_notifier_count_value(x) ((x)->value)

int faio_notifier_manager_init(faio_notifier_manager_t *notifier, 
    faio_manager_t *fm, faio_errno_t *err_no);
int faio_notifier_manager_release(faio_notifier_manager_t *notifier, 
    faio_errno_t *err_no);
int faio_notifier_send(faio_notifier_manager_t *notifier, 
    faio_errno_t *err_no);
int faio_notifier_receive(faio_notifier_manager_t *notifier, 
    faio_errno_t *err_no);
int faio_notifier_count_inc(faio_notifier_manager_t *notifier, 
    faio_errno_t *err_no);
int faio_notifier_count_dec(faio_notifier_manager_t *notifier, 
    faio_errno_t *err_no);

#endif

