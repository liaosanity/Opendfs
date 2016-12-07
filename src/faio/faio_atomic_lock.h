#ifndef FAIO_ATOMIC_LOCK_H
#define FAIO_ATOMIC_LOCK_H

#include <stdint.h>

#define FAIO_ATOMIC_LOCK_ON     (2)
#define FAIO_ATOMIC_LOCK_OFF    (1)

typedef struct 
{
    volatile uint64_t lock;
} faio_atomic_lock_t;

#define faio_atomic_lock_init(atomic_lock) \
    (atomic_lock).lock = FAIO_ATOMIC_LOCK_OFF

int faio_atomic_lock(faio_atomic_lock_t *atomic_lock);
int faio_atomic_unlock(faio_atomic_lock_t *atomic_lock);

#endif

