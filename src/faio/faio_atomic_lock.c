#include "faio_atomic_lock.h"

#if (__GNUC__ >= 4 && __GNUC_MINOR__ >=2 )
#define CAS(val, old, set) \
    __sync_bool_compare_and_swap((val), (old), (set))
#else
#define CAS(val, old, set) \
	cmp_and_swap((val), (old), (set))
#endif

int faio_atomic_lock(faio_atomic_lock_t *atomic_lock)
{
    while (!CAS(&atomic_lock->lock, FAIO_ATOMIC_LOCK_OFF, FAIO_ATOMIC_LOCK_ON));
    
    return FAIO_ATOMIC_LOCK_ON;
}

int faio_atomic_unlock(faio_atomic_lock_t *atomic_lock)
{
    if (atomic_lock->lock == FAIO_ATOMIC_LOCK_OFF) 
	{
    	return FAIO_ATOMIC_LOCK_OFF;
    }
    
    while (!CAS(&atomic_lock->lock, FAIO_ATOMIC_LOCK_ON, FAIO_ATOMIC_LOCK_OFF));
    
    return FAIO_ATOMIC_LOCK_OFF;
}

