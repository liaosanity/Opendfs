#include <errno.h>

#include "dfs_lock.h"

#define DFS_LOCK_ERRNO_CLEAN(e)                \
    (e)->lock_errno = DFS_LOCK_ERR_NONE;       \
    (e)->syscall_errno = 0;                    \
    (e)->allocator_errno = DFS_LOCK_ERR_NONE;  \
    (e)->err_allocator = NULL

#define DFS_LOCK_ERRNO_ALLOCATOR(e, a)         \
    (e)->err_allocator = (a)

/*
 * Special Note:
 * When the instances of dfs_process_lock are more than one
 * the dfs_process_lock_held assignment must be confused!!!!!!
 */

static const char *dfs_lock_err_string[] = 
{
    "dfs_lock: unknown error number",
    "dfs_lock: parameter error",
    "dfs_lock: allocator error",
    "dfs_lock: mutex attribute init error",
    "dfs_lock: mutex attribute setpshared error",
    "dfs_lock: mutex init error",
    "dfs_lock: mutex attribute destroy error",
    "dfs_lock: mutex destroy error",
    "dfs_lock: mutex lock error", 
    "dfs_lock: mutex try lock error",
    "dfs_lock: mutex unlock error",
    "dfs_lock: sigprocmask error",
    "dfs_lock: rwlock attribute init error",
    "dfs_lock: rwlock attribute setpshared error",
    "dfs_lock: rwlock init error",
    "dfs_lock: rwlock attribute destroy error",
    "dfs_lock: rwlock destroy error",
    "dfs_lock: rwlock reading lock error",
    "dfs_lock: rwlock unlock error",
    "dfs_lock: rwlock writing lock error",
    "dfs_lock: rwlock writing try lock error",
    NULL
};

static inline unsigned char cmp_and_swap(volatile uint64_t *ptr, 
	                                             uint64_t old, uint64_t nw)
{
    unsigned char ret = 0;
	
    __asm__ __volatile__("lock; cmpxchgq %1,%2; setz %0"
        : "=a"(ret)
        : "q"(nw), "m"(*(volatile uint64_t *)(ptr)), "0"(old)
        : "memory");
	
    return ret;
}

dfs_process_lock_t * dfs_process_lock_create(dfs_mem_allocator_t *allocator, 
	                                                 dfs_lock_errno_t *error)
{
    dfs_process_lock_t *process_lock = NULL;

    if (!error) 
	{
        return NULL;
    }

    DFS_LOCK_ERRNO_CLEAN(error);
	
    if (!allocator) 
	{
        error->lock_errno = DFS_LOCK_ERR_PARAM;
		
        return NULL;
    }

    process_lock = (dfs_process_lock_t *)allocator->alloc(allocator, sizeof(dfs_process_lock_t),
        &error->allocator_errno);
    if (!process_lock) 
	{
        error->lock_errno = DFS_LOCK_ERR_ALLOCATOR_ERROR;
		
        return NULL;
    }
	
    process_lock->allocator = allocator;

    if ((error->syscall_errno =
             pthread_mutexattr_init(&process_lock->mutex_attr))) 
    {
        error->lock_errno = DFS_LOCK_ERR_SYSCALL_MUTEXATTR_INIT;
		
        return NULL;
    }

    if ((error->syscall_errno =
            pthread_mutexattr_setpshared(&process_lock->mutex_attr,
    	    PTHREAD_PROCESS_SHARED))) 
    {
        error->lock_errno = DFS_LOCK_ERR_SYSCALL_MUTEXATTR_SETPSHARED;
		
        return NULL;
    }

    if ((error->syscall_errno = pthread_mutex_init(&process_lock->lock, 
        &process_lock->mutex_attr))) 
    {
        error->lock_errno = DFS_LOCK_ERR_SYSCALL_MUTEX_INIT;
		
        return NULL;
    }

    sigfillset(&process_lock->sig_block_mask);
    sigdelset(&process_lock->sig_block_mask, SIGALRM);
    sigdelset(&process_lock->sig_block_mask, SIGINT);
    sigdelset(&process_lock->sig_block_mask, SIGCHLD);
    sigdelset(&process_lock->sig_block_mask, SIGPIPE);
    sigdelset(&process_lock->sig_block_mask, SIGSEGV);
    sigdelset(&process_lock->sig_block_mask, SIGHUP);
    sigdelset(&process_lock->sig_block_mask, SIGQUIT);
    sigdelset(&process_lock->sig_block_mask, SIGTERM);
    sigdelset(&process_lock->sig_block_mask, SIGIO);
    sigdelset(&process_lock->sig_block_mask, SIGUSR1);

    return process_lock;    
}

int dfs_process_lock_release(dfs_process_lock_t *process_lock,
                                      dfs_lock_errno_t *error)
{
    dfs_mem_allocator_t *allocator = NULL; 

    if (!error) 
	{
        return DFS_LOCK_ERROR;
    }

    DFS_LOCK_ERRNO_CLEAN(error);
	
	if (!process_lock || !process_lock->allocator) 
	{
        error->lock_errno = DFS_LOCK_ERR_PARAM;
		
		return DFS_LOCK_ERROR;
	}
	
    DFS_LOCK_ERRNO_ALLOCATOR(error, process_lock->allocator);

    if (sigprocmask(SIG_SETMASK, &process_lock->sig_prev_mask, NULL)) 
	{
        error->lock_errno = DFS_LOCK_ERR_SYSCALL_SIGPROCMASK;
        error->syscall_errno = errno;
		
        return DFS_LOCK_ERROR;
    }
	
    if ((error->syscall_errno =
        pthread_mutexattr_destroy(&process_lock->mutex_attr))) 
    {
        error->lock_errno = DFS_LOCK_ERR_SYSCALL_MUTEXATTR_DESTROY;
		
    	return DFS_LOCK_ERROR;
    }
	
    if ((error->syscall_errno =
        pthread_mutex_destroy(&process_lock->lock))) 
    {
        error->lock_errno = DFS_LOCK_ERR_SYSCALL_MUTEX_DESTROY;
		
    	return DFS_LOCK_ERROR;
    }

    allocator = process_lock->allocator;
	
    if (allocator->free(allocator, process_lock, &error->allocator_errno)
        == DFS_MEM_ALLOCATOR_ERROR) {
        error->lock_errno = DFS_LOCK_ERR_ALLOCATOR_ERROR;
    }
	
    return DFS_LOCK_OK;
}

int dfs_process_lock_reset(dfs_process_lock_t *process_lock,
                                   dfs_lock_errno_t *error)
{
	if (!error) 
	{
	    return DFS_LOCK_ERROR;
    }
	
    DFS_LOCK_ERRNO_CLEAN(error);
	
	if (!process_lock) 
	{
        error->lock_errno = DFS_LOCK_ERR_PARAM;
		
		return DFS_LOCK_ERROR;
    }
	
    DFS_LOCK_ERRNO_ALLOCATOR(error, process_lock->allocator);
	
    if ((error->syscall_errno = pthread_mutex_init(&process_lock->lock, 
        &process_lock->mutex_attr))) 
    {
        error->lock_errno = DFS_LOCK_ERR_SYSCALL_MUTEX_INIT;
		
        return DFS_LOCK_ERROR;
    }

    return DFS_LOCK_OK;
}

int dfs_process_lock_on(dfs_process_lock_t *process_lock,
                                dfs_lock_errno_t *error)
{
    if (!error) 
	{
        return DFS_LOCK_ERROR;
    }
    
    DFS_LOCK_ERRNO_CLEAN(error);
	
	if (!process_lock) 
	{
        error->lock_errno = DFS_LOCK_ERR_PARAM;
		
        return DFS_LOCK_ERROR;
    }
	
    DFS_LOCK_ERRNO_ALLOCATOR(error, process_lock->allocator);

    if (sigprocmask(SIG_BLOCK, &process_lock->sig_block_mask,
    	&process_lock->sig_prev_mask)) 
    {
    	error->lock_errno = DFS_LOCK_ERR_SYSCALL_SIGPROCMASK;   
    	error->syscall_errno = errno;
		
    	return DFS_LOCK_ERROR;
    }
    
    if ((error->syscall_errno =
        pthread_mutex_lock(&process_lock->lock))) 
    {
    	error->lock_errno = DFS_LOCK_ERR_SYSCALL_MUTEX_LOCK;
		
    	return DFS_LOCK_ERROR;
    }
    
    return DFS_LOCK_ON;
}

int dfs_process_lock_try_on(dfs_process_lock_t *process_lock, 
	                                 dfs_lock_errno_t *error)
{
    if (!error) 
	{
        return DFS_LOCK_ERROR;
    }

    DFS_LOCK_ERRNO_CLEAN(error);
	
	if (!process_lock) 
	{
	    error->lock_errno = DFS_LOCK_ERR_PARAM; 
		
        return DFS_LOCK_ERROR;
    }
	
    DFS_LOCK_ERRNO_ALLOCATOR(error, process_lock->allocator);

    if (sigprocmask(SIG_BLOCK, &process_lock->sig_block_mask,
        &process_lock->sig_prev_mask)) 
    {
    	error->lock_errno = DFS_LOCK_ERR_SYSCALL_SIGPROCMASK;   
    	error->syscall_errno = errno;
		
        return DFS_LOCK_ERROR;
    }
    
    if ((error->syscall_errno =
        pthread_mutex_trylock(&process_lock->lock))) 
    {
    	error->lock_errno = DFS_LOCK_ERR_SYSCALL_MUTEX_TRYLOCK;
		
        return DFS_LOCK_OFF;
    }
    
    return DFS_LOCK_ON;

}

int dfs_process_lock_off(dfs_process_lock_t *process_lock,
                                dfs_lock_errno_t *error)
{
    if (!error) 
	{
        return DFS_LOCK_ERROR;
    }
    
    DFS_LOCK_ERRNO_CLEAN(error);
	
	if (!process_lock) 
	{
	    error->lock_errno = DFS_LOCK_ERR_PARAM;
		
        return DFS_LOCK_ERROR;
    }
	
    DFS_LOCK_ERRNO_ALLOCATOR(error, process_lock->allocator);
	
    if ((error->syscall_errno =
        pthread_mutex_unlock(&process_lock->lock))) 
    {
        error->lock_errno = DFS_LOCK_ERR_SYSCALL_MUTEX_UNLOCK;
		
    	return DFS_LOCK_ERROR;
    }

    if (sigprocmask(SIG_SETMASK, &process_lock->sig_prev_mask, NULL)) 
	{
    	error->lock_errno = DFS_LOCK_ERR_SYSCALL_SIGPROCMASK;   
    	error->syscall_errno = errno;
		
    	return DFS_LOCK_ERROR;
    }
    
    return DFS_LOCK_OFF;
}

dfs_atomic_lock_t * dfs_atomic_lock_create(dfs_mem_allocator_t *allocator,
                                                   dfs_lock_errno_t *error)
{
    dfs_atomic_lock_t *atomic_lock = NULL; 	
    
    if (!error) 
	{
        return NULL;
    }
	
    DFS_LOCK_ERRNO_CLEAN(error);
    DFS_LOCK_ERRNO_ALLOCATOR(error, allocator);
	
    if (!allocator) 
	{
        error->lock_errno = DFS_LOCK_ERR_PARAM;
		
        return NULL;
    }
    
    atomic_lock = (dfs_atomic_lock_t *)allocator->alloc(allocator, sizeof(dfs_atomic_lock_t),
        &error->allocator_errno);
	if (!atomic_lock) 
	{
	    error->lock_errno = DFS_LOCK_ERR_ALLOCATOR_ERROR;
		
		return NULL;
    }
	
    atomic_lock->allocator = allocator; 
    atomic_lock->lock=DFS_LOCK_OFF; 
    
    return atomic_lock;
}

int dfs_atomic_lock_init(dfs_atomic_lock_t *atomic_lock)
{
    atomic_lock->lock = DFS_LOCK_OFF; 
    
    return DFS_LOCK_OK;
}

int dfs_atomic_lock_release(dfs_atomic_lock_t *atomic_lock,
                                    dfs_lock_errno_t *error)
{
    dfs_mem_allocator_t *allocator = NULL;
	
    if (!error) 
	{
        return DFS_LOCK_ERROR;
    }
	
    DFS_LOCK_ERRNO_CLEAN(error);
	
	if (!atomic_lock || !atomic_lock->allocator) 
	{
	    error->lock_errno = DFS_LOCK_ERR_PARAM;
		
		return DFS_LOCK_ERROR;
    }
	
    DFS_LOCK_ERRNO_ALLOCATOR(error, atomic_lock->allocator);
	
    allocator = atomic_lock->allocator; 
    if (allocator->free(allocator, atomic_lock, &error->allocator_errno)
        == DFS_MEM_ALLOCATOR_ERROR) 
    {
        error->lock_errno = DFS_LOCK_ERR_ALLOCATOR_ERROR;
		
        return DFS_LOCK_ERROR;
    }
    
    return DFS_LOCK_OK;
}

int dfs_atomic_lock_reset(dfs_atomic_lock_t *atomic_lock,
                                 dfs_lock_errno_t *error) 
{
    if (!error) 
	{
        return DFS_LOCK_ERROR;
    }
	
    DFS_LOCK_ERRNO_CLEAN(error);
	
	if (!atomic_lock) 
	{
	    error->lock_errno = DFS_LOCK_ERR_PARAM;
		
		return DFS_LOCK_ERROR;
    }
	
    DFS_LOCK_ERRNO_ALLOCATOR(error, atomic_lock->allocator);
    atomic_lock->lock = DFS_LOCK_OFF;
	
    return DFS_LOCK_OK;
}

int dfs_atomic_lock_try_on(dfs_atomic_lock_t *atomic_lock,
                                   dfs_lock_errno_t *error)
{
    if (!error) 
	{
        return DFS_LOCK_ERROR;
    }
	
    DFS_LOCK_ERRNO_CLEAN(error);
	
	if (!atomic_lock) 
	{
        error->lock_errno = DFS_LOCK_ERR_PARAM;
		
		return DFS_LOCK_ERROR;
    }
	
    DFS_LOCK_ERRNO_ALLOCATOR(error, atomic_lock->allocator);

    return atomic_lock->lock == DFS_LOCK_OFF
        && CAS(&atomic_lock->lock, DFS_LOCK_OFF, DFS_LOCK_ON) ?
        DFS_LOCK_ON : DFS_LOCK_OFF;
}

int dfs_atomic_lock_on(dfs_atomic_lock_t *atomic_lock,
                              dfs_lock_errno_t *error)
{
    if (!error) 
	{
        return DFS_LOCK_ERROR;
    }
	
    DFS_LOCK_ERRNO_CLEAN(error);
	
	if (!atomic_lock) 
	{
	    error->lock_errno = DFS_LOCK_ERR_PARAM;
		
		return DFS_LOCK_ERROR;
    }
	
    DFS_LOCK_ERRNO_ALLOCATOR(error, atomic_lock->allocator);
    
    while (!CAS(&atomic_lock->lock, DFS_LOCK_OFF, DFS_LOCK_ON));
    
    return DFS_LOCK_ON;
}

int dfs_atomic_lock_off(dfs_atomic_lock_t *atomic_lock,
                              dfs_lock_errno_t *error)
{
    if (!error) 
	{
        return DFS_LOCK_ERROR;
    }
	
    DFS_LOCK_ERRNO_CLEAN(error);
	
	if (!atomic_lock) 
	{
        error->lock_errno = DFS_LOCK_ERR_PARAM;
		
		return DFS_LOCK_ERROR;
    }
	
    DFS_LOCK_ERRNO_ALLOCATOR(error, atomic_lock->allocator);
    
    if (atomic_lock->lock == DFS_LOCK_OFF) 
	{
    	return DFS_LOCK_OFF;
    }
    
    while (!CAS(&atomic_lock->lock, DFS_LOCK_ON, DFS_LOCK_OFF));
    
    return DFS_LOCK_OFF;
}

dfs_process_rwlock_t* dfs_process_rwlock_create(dfs_mem_allocator_t *allocator,
                                                         dfs_lock_errno_t *error)
{
    dfs_process_rwlock_t *process_rwlock = NULL;

    if (!error) 
	{
        return NULL;
    }

    DFS_LOCK_ERRNO_CLEAN(error);
    DFS_LOCK_ERRNO_ALLOCATOR(error, allocator);

    if (!allocator) 
	{
        error->lock_errno = DFS_LOCK_ERR_PARAM;
		
        return NULL;
    }
	
    process_rwlock = (dfs_process_rwlock_t *)allocator->alloc(allocator, sizeof(dfs_process_rwlock_t),
        &error->allocator_errno);
    if (!process_rwlock) 
	{
        error->lock_errno = DFS_LOCK_ERR_ALLOCATOR_ERROR;
		
        return NULL;
    }
	
    if ((error->syscall_errno =
        pthread_rwlockattr_init(&process_rwlock->rwlock_attr))) 
    {
        error->lock_errno = DFS_LOCK_ERR_SYSCALL_RWLOCKATTR_INIT;
		
        return NULL;
    }

    if ((error->syscall_errno =
        pthread_rwlockattr_setpshared(&process_rwlock->rwlock_attr,
    	PTHREAD_PROCESS_SHARED))) 
    {
        error->lock_errno = DFS_LOCK_ERR_SYSCALL_RWLOCKATTR_SETPSHARED;
		
        return NULL;
    }

    if ((error->syscall_errno = pthread_rwlock_init(&process_rwlock->rwlock,
    	&process_rwlock->rwlock_attr))) 
    {
        error->lock_errno = DFS_LOCK_ERR_SYSCALL_RWLOCK_INIT;
		
        return NULL;
    }
    
    process_rwlock->allocator = allocator;

    sigfillset(&process_rwlock->sig_block_mask);
    sigdelset(&process_rwlock->sig_block_mask, SIGALRM);
    sigdelset(&process_rwlock->sig_block_mask, SIGINT);
    sigdelset(&process_rwlock->sig_block_mask, SIGCHLD);
    sigdelset(&process_rwlock->sig_block_mask, SIGPIPE);
    sigdelset(&process_rwlock->sig_block_mask, SIGSEGV);
    sigdelset(&process_rwlock->sig_block_mask, SIGHUP);
    sigdelset(&process_rwlock->sig_block_mask, SIGQUIT);
    sigdelset(&process_rwlock->sig_block_mask, SIGTERM);
    sigdelset(&process_rwlock->sig_block_mask, SIGIO);
    sigdelset(&process_rwlock->sig_block_mask, SIGUSR1);

    return process_rwlock;    
}

int dfs_process_rwlock_release(dfs_process_rwlock_t* process_rwlock,
                                         dfs_lock_errno_t *error)
{
    dfs_mem_allocator_t *allocator = NULL;
	
    if (!error) 
	{
        return DFS_LOCK_ERROR;
    }
    
    DFS_LOCK_ERRNO_CLEAN(error);
	
	if (!process_rwlock || !process_rwlock->allocator) 
	{
        error->lock_errno = DFS_LOCK_ERR_PARAM;
		
		return DFS_LOCK_ERROR;
	}
	
    DFS_LOCK_ERRNO_ALLOCATOR(error, process_rwlock->allocator);

    if (sigprocmask(SIG_SETMASK, &process_rwlock->sig_prev_mask, NULL)) 
	{
        error->lock_errno = DFS_LOCK_ERR_SYSCALL_SIGPROCMASK;
        error->syscall_errno = errno;
		
        return DFS_LOCK_ERROR;
    }
    
    if ((error->syscall_errno =
        pthread_rwlockattr_destroy(&process_rwlock->rwlock_attr))) 
    {
        error->lock_errno = DFS_LOCK_ERR_SYSCALL_RWLOCKATTR_DESTROY;
		
        return DFS_LOCK_ERROR;
    }
    
    if ((error->syscall_errno =
         pthread_rwlock_destroy(&process_rwlock->rwlock))) 
    {
        error->lock_errno = DFS_LOCK_ERR_SYSCALL_RWLOCK_DESTROY;
		
    	return DFS_LOCK_ERROR;
    }
	
    allocator = process_rwlock->allocator; 
    if (allocator->free(allocator, process_rwlock, &error->allocator_errno)
        == DFS_MEM_ALLOCATOR_ERROR) 
    {
        error->lock_errno = DFS_LOCK_ERR_ALLOCATOR_ERROR;
		
        return DFS_LOCK_ERROR;
    }
    
    return DFS_LOCK_OK;
}

int dfs_process_rwlock_reset(dfs_process_rwlock_t *process_rwlock,
                                      dfs_lock_errno_t *error)
{
    if (!error) 
	{
        return DFS_LOCK_ERROR;
    }
	
    DFS_LOCK_ERRNO_CLEAN(error);
	
	if (!process_rwlock) 
	{
        error->lock_errno = DFS_LOCK_ERR_PARAM;
		
		return DFS_LOCK_ERROR;
    }
	
    DFS_LOCK_ERRNO_ALLOCATOR(error, process_rwlock->allocator);

    if ((error->syscall_errno = pthread_rwlock_init(&process_rwlock->rwlock,
    	&process_rwlock->rwlock_attr))) 
    {
    	error->lock_errno = DFS_LOCK_ERR_SYSCALL_RWLOCK_INIT;
		
        return DFS_LOCK_ERROR;
    }
	
    return DFS_LOCK_OK;
}

int dfs_process_rwlock_read_on(dfs_process_rwlock_t *process_rwlock,
                                          dfs_lock_errno_t *error)
{
    if (!error) 
	{
        return DFS_LOCK_ERROR;
    }
	
    DFS_LOCK_ERRNO_CLEAN(error);
	
	if (!process_rwlock) 
	{
        error->lock_errno = DFS_LOCK_ERR_PARAM;
		
        return DFS_LOCK_ERROR;
    }
	
    DFS_LOCK_ERRNO_ALLOCATOR(error, process_rwlock->allocator);

    if (sigprocmask(SIG_BLOCK, &process_rwlock->sig_block_mask,
    	&process_rwlock->sig_prev_mask)) 
    {
    	error->lock_errno = DFS_LOCK_ERR_SYSCALL_SIGPROCMASK;
    	error->syscall_errno = errno;
		
    	return DFS_LOCK_ERROR;
    }
	
    if ((error->syscall_errno =
        pthread_rwlock_rdlock(&process_rwlock->rwlock))) 
    {
        error->lock_errno = DFS_LOCK_ERR_SYSCALL_RWLOCK_RDLOCK;
		
    	return DFS_LOCK_ERROR;
    }
	
    return DFS_LOCK_ON;
}

int dfs_process_rwlock_off(dfs_process_rwlock_t *process_rwlock,
                                   dfs_lock_errno_t *error)
{
    if (!error) 
	{
        return DFS_LOCK_ERROR;
    }
    
    DFS_LOCK_ERRNO_CLEAN(error);
	
	if (!process_rwlock) 
	{
        error->lock_errno = DFS_LOCK_ERR_PARAM;
		
        return DFS_LOCK_ERROR;
    }
	
    DFS_LOCK_ERRNO_ALLOCATOR(error, process_rwlock->allocator);
    
    if ((error->syscall_errno = pthread_rwlock_unlock(&process_rwlock->rwlock)))
	{
        error->lock_errno = DFS_LOCK_ERR_SYSCALL_RWLOCK_UNLOCK;
		
    	return DFS_LOCK_ERROR;
    }

    if (sigprocmask(SIG_SETMASK, &process_rwlock->sig_prev_mask, NULL)) 
	{
    	error->lock_errno = DFS_LOCK_ERR_SYSCALL_SIGPROCMASK;
    	error->syscall_errno = errno;
		
    	return DFS_LOCK_ERROR;
    }
    
    return DFS_LOCK_OFF;
}

int dfs_process_rwlock_write_on(dfs_process_rwlock_t *process_rwlock,
                                           dfs_lock_errno_t *error)
{
    if (!error) 
	{
        return DFS_LOCK_ERROR;
    }
    
    DFS_LOCK_ERRNO_CLEAN(error);
	
    error->lock_errno = DFS_LOCK_ERR_NONE;
	
	if (!process_rwlock) 
	{
        error->lock_errno = DFS_LOCK_ERR_PARAM;
        return DFS_LOCK_ERROR;
    }
	
    DFS_LOCK_ERRNO_ALLOCATOR(error, process_rwlock->allocator);

    if (sigprocmask(SIG_BLOCK, &process_rwlock->sig_block_mask,
    	&process_rwlock->sig_prev_mask)) 
    {
    	error->lock_errno = DFS_LOCK_ERR_SYSCALL_SIGPROCMASK;
    	error->syscall_errno = errno;
		
    	return DFS_LOCK_ERROR;
    }
    
    if ((error->syscall_errno =
        pthread_rwlock_wrlock(&process_rwlock->rwlock))) 
    {
        error->lock_errno = DFS_LOCK_ERR_SYSCALL_RWLOCK_WRLOCK;
		
    	return DFS_LOCK_ERROR;
    }
    
    return DFS_LOCK_ON;
}

int dfs_process_rwlock_write_try_on(dfs_process_rwlock_t *process_rwlock,
                                               dfs_lock_errno_t *error)
{
    if (!error) 
	{
        return DFS_LOCK_ERROR;
    }
    
    DFS_LOCK_ERRNO_CLEAN(error);
	
	if (!process_rwlock) 
	{
        error->lock_errno = DFS_LOCK_ERR_PARAM;
		
        return DFS_LOCK_ERROR;
    }
	
    DFS_LOCK_ERRNO_ALLOCATOR(error, process_rwlock->allocator);
	
    if (sigprocmask(SIG_BLOCK, &process_rwlock->sig_block_mask,
    	&process_rwlock->sig_prev_mask)) 
    {
    	error->lock_errno = DFS_LOCK_ERR_SYSCALL_SIGPROCMASK;
    	error->syscall_errno = errno;
		
    	return DFS_LOCK_ERROR;
    }
    
    if ((error->syscall_errno =
        pthread_rwlock_trywrlock(&process_rwlock->rwlock))) 
    {
        error->lock_errno = DFS_LOCK_ERR_SYSCALL_RWLOCK_TRYWRLOCK;
		
    	return DFS_LOCK_ERROR;
    }
    
    return DFS_LOCK_ON;
}

const char* dfs_lock_strerror(dfs_lock_errno_t *error)
{
    if (!error) 
	{
        return dfs_lock_err_string[0];
    }

    if (error->lock_errno <= DFS_LOCK_ERR_START
        || error->lock_errno >= DFS_LOCK_ERR_END) 
    {
        return dfs_lock_err_string[0];
    }

    if (error->lock_errno == DFS_LOCK_ERR_ALLOCATOR_ERROR
        && error->err_allocator
        && error->err_allocator->strerror) 
    {
        return error->err_allocator->strerror(error->err_allocator,
            &error->allocator_errno);
    }
	
    if (error->lock_errno >= DFS_LOCK_ERR_SYSCALL_MUTEXATTR_INIT
        && error->lock_errno <= DFS_LOCK_ERR_SYSCALL_RWLOCK_TRYWRLOCK) 
    {
        return strerror(error->syscall_errno);
    }

    return dfs_lock_err_string[error->lock_errno - DFS_LOCK_ERR_START];
}

inline void dfs_atomic_lock_off_force(dfs_atomic_lock_t *atomic_lock)
{
    atomic_lock->lock = DFS_LOCK_OFF;
}

dfs_atomic_lock_t * dfs_atomic_lock_pool_create(pool_t *pool)
{
    dfs_atomic_lock_t *atomic_lock = NULL;
	
    if (!pool) 
	{
        return NULL;
    }
	
    atomic_lock = (dfs_atomic_lock_t *)pool_alloc(pool, sizeof(dfs_atomic_lock_t));
	if (!atomic_lock) 
	{
		return NULL;
    }
	
    atomic_lock->lock = DFS_LOCK_OFF; 
	
    return atomic_lock;
}

int dfs_process_rwlock_init(dfs_process_rwlock_t * process_rwlock,
                                    dfs_lock_errno_t *error)
{
    if (!error) 
	{
        return DFS_LOCK_ERROR;
    }

    if ((error->syscall_errno =
        pthread_rwlockattr_init(&process_rwlock->rwlock_attr))) 
    {
        error->lock_errno = DFS_LOCK_ERR_SYSCALL_RWLOCKATTR_INIT;
		
        return DFS_LOCK_ERROR;
    }

    if ((error->syscall_errno =
        pthread_rwlockattr_setpshared(&process_rwlock->rwlock_attr,
    	PTHREAD_PROCESS_SHARED))) 
    {
        error->lock_errno = DFS_LOCK_ERR_SYSCALL_RWLOCKATTR_SETPSHARED;
		
        return DFS_LOCK_ERROR;
    }

    if ((error->syscall_errno = pthread_rwlock_init(&process_rwlock->rwlock,
    	&process_rwlock->rwlock_attr))) 
    {
        error->lock_errno = DFS_LOCK_ERR_SYSCALL_RWLOCK_INIT;
		
        return DFS_LOCK_ERROR;
    }
    
    sigfillset(&process_rwlock->sig_block_mask);
    sigdelset(&process_rwlock->sig_block_mask, SIGALRM);
    sigdelset(&process_rwlock->sig_block_mask, SIGINT);
    sigdelset(&process_rwlock->sig_block_mask, SIGCHLD);
    sigdelset(&process_rwlock->sig_block_mask, SIGPIPE);
    sigdelset(&process_rwlock->sig_block_mask, SIGSEGV);
    sigdelset(&process_rwlock->sig_block_mask, SIGHUP);
    sigdelset(&process_rwlock->sig_block_mask, SIGQUIT);
    sigdelset(&process_rwlock->sig_block_mask, SIGTERM);
    sigdelset(&process_rwlock->sig_block_mask, SIGIO);
    sigdelset(&process_rwlock->sig_block_mask, SIGUSR1);

    return DFS_LOCK_OK;    
}

int dfs_process_rwlock_destroy(dfs_process_rwlock_t* process_rwlock,
                                         dfs_lock_errno_t *error)
{
    if (sigprocmask(SIG_SETMASK, &process_rwlock->sig_prev_mask, NULL)) 
	{
        error->lock_errno = DFS_LOCK_ERR_SYSCALL_SIGPROCMASK;
        error->syscall_errno = errno;
		
        return DFS_LOCK_ERROR;
    }
    
    if ((error->syscall_errno =
        pthread_rwlockattr_destroy(&process_rwlock->rwlock_attr))) 
    {
        error->lock_errno = DFS_LOCK_ERR_SYSCALL_RWLOCKATTR_DESTROY;
		
        return DFS_LOCK_ERROR;
    }
    
    if ((error->syscall_errno =
         pthread_rwlock_destroy(&process_rwlock->rwlock))) 
    {
        error->lock_errno = DFS_LOCK_ERR_SYSCALL_RWLOCK_DESTROY;
		
    	return DFS_LOCK_ERROR;
    }
      
    return DFS_LOCK_OK;
}

