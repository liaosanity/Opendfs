#ifndef DFS_LOCK_H
#define DFS_LOCK_H

#include <pthread.h>
#include <unistd.h>
#include <stdint.h>
#include <signal.h>
#include <string.h>

#include "dfs_mem_allocator.h"
#include "dfs_memory_pool.h"

#define	DFS_LOCK_ERROR -1
#define DFS_LOCK_OK     0
#define	DFS_LOCK_OFF    1
#define	DFS_LOCK_ON     2

#define DFS_LOCK_FALSE  0
#define DFS_LOCK_TRUE   1

#if (__GNUC__ >= 4 && __GNUC_MINOR__ >=2 )
#define CAS(val, old, set) \
    __sync_bool_compare_and_swap((val), (old), (set))
#else
#define CAS(val, old, set) \
	cmp_and_swap((val), (old), (set))
#endif

enum 
{
    DFS_LOCK_ERR_NONE = 0,
    DFS_LOCK_ERR_START = 100,
    DFS_LOCK_ERR_PARAM,
    DFS_LOCK_ERR_ALLOCATOR_ERROR,
    DFS_LOCK_ERR_SYSCALL_MUTEXATTR_INIT,
    DFS_LOCK_ERR_SYSCALL_MUTEXATTR_SETPSHARED,
    DFS_LOCK_ERR_SYSCALL_MUTEX_INIT,
    DFS_LOCK_ERR_SYSCALL_MUTEXATTR_DESTROY,
    DFS_LOCK_ERR_SYSCALL_MUTEX_DESTROY,
    DFS_LOCK_ERR_SYSCALL_MUTEX_LOCK,
    DFS_LOCK_ERR_SYSCALL_MUTEX_TRYLOCK,
    DFS_LOCK_ERR_SYSCALL_MUTEX_UNLOCK,
    DFS_LOCK_ERR_SYSCALL_SIGPROCMASK,
    DFS_LOCK_ERR_SYSCALL_RWLOCKATTR_INIT,
    DFS_LOCK_ERR_SYSCALL_RWLOCKATTR_SETPSHARED,
    DFS_LOCK_ERR_SYSCALL_RWLOCK_INIT,
    DFS_LOCK_ERR_SYSCALL_RWLOCKATTR_DESTROY,
    DFS_LOCK_ERR_SYSCALL_RWLOCK_DESTROY,
    DFS_LOCK_ERR_SYSCALL_RWLOCK_RDLOCK,
    DFS_LOCK_ERR_SYSCALL_RWLOCK_UNLOCK,
    DFS_LOCK_ERR_SYSCALL_RWLOCK_WRLOCK,
    DFS_LOCK_ERR_SYSCALL_RWLOCK_TRYWRLOCK,
    DFS_LOCK_ERR_END
};

typedef struct 
{
	pthread_mutex_t      lock;
	pthread_mutexattr_t  mutex_attr;
    sigset_t             sig_block_mask;
    sigset_t             sig_prev_mask;
	dfs_mem_allocator_t *allocator;
} dfs_process_lock_t;

typedef struct 
{
	pthread_rwlock_t     rwlock;
	pthread_rwlockattr_t rwlock_attr;
	sigset_t             sig_block_mask;
	sigset_t             sig_prev_mask;
	dfs_mem_allocator_t *allocator;
} dfs_process_rwlock_t;

typedef struct 
{
    volatile uint64_t    lock;
	dfs_mem_allocator_t *allocator;
} dfs_atomic_lock_t;

typedef struct 
{
    int                  syscall_errno;
    unsigned int         lock_errno;
    unsigned int         allocator_errno;
    dfs_mem_allocator_t *err_allocator;
} dfs_lock_errno_t;

dfs_process_lock_t * dfs_process_lock_create(dfs_mem_allocator_t *allocator,
    dfs_lock_errno_t *error);
int dfs_process_lock_release(dfs_process_lock_t *process_lock,
    dfs_lock_errno_t *error);
int dfs_process_lock_reset(dfs_process_lock_t *process_lock,
    dfs_lock_errno_t *error);
int dfs_process_lock_on(dfs_process_lock_t *process_lock,
    dfs_lock_errno_t *error);
int dfs_process_lock_off(dfs_process_lock_t *process_lock,
    dfs_lock_errno_t *error);
int dfs_process_lock_try_on(dfs_process_lock_t *process_lock,
    dfs_lock_errno_t *error);
dfs_process_rwlock_t * dfs_process_rwlock_create(dfs_mem_allocator_t *allocator,
    dfs_lock_errno_t *error);
int dfs_process_rwlock_release(dfs_process_rwlock_t *process_rwlock,
    dfs_lock_errno_t *error);
int dfs_process_rwlock_init(dfs_process_rwlock_t * process_rwlock,
    dfs_lock_errno_t *error);
int dfs_process_rwlock_destroy(dfs_process_rwlock_t* process_rwlock,
    dfs_lock_errno_t *error);
int dfs_process_rwlock_reset(dfs_process_rwlock_t *process_rwlock,
    dfs_lock_errno_t *error);
int dfs_process_rwlock_read_on(dfs_process_rwlock_t *process_rwlock,
    dfs_lock_errno_t *error);
int dfs_process_rwlock_write_on(dfs_process_rwlock_t *process_rwlock,
    dfs_lock_errno_t *error);
int dfs_process_rwlock_write_try_on(dfs_process_rwlock_t *process_rwlock,
    dfs_lock_errno_t *error);
int dfs_process_rwlock_off(dfs_process_rwlock_t *process_rwlock,
    dfs_lock_errno_t *error);
dfs_atomic_lock_t * dfs_atomic_lock_create(dfs_mem_allocator_t *allocator,
    dfs_lock_errno_t *error);
int dfs_atomic_lock_init(dfs_atomic_lock_t *atomic_lock);
int dfs_atomic_lock_release(dfs_atomic_lock_t *atomic_lock,
    dfs_lock_errno_t *error);
int dfs_atomic_lock_reset(dfs_atomic_lock_t *atomic_lock,
    dfs_lock_errno_t *error);
int dfs_atomic_lock_try_on(dfs_atomic_lock_t *atomic_lock,
    dfs_lock_errno_t *error);
int dfs_atomic_lock_on(dfs_atomic_lock_t *atomic_lock,
    dfs_lock_errno_t *error);
int dfs_atomic_lock_off(dfs_atomic_lock_t *atomic_lock,
    dfs_lock_errno_t *error);
const char * dfs_lock_strerror(dfs_lock_errno_t *error);
inline void dfs_atomic_lock_off_force(dfs_atomic_lock_t *atomic_lock);
dfs_atomic_lock_t *dfs_atomic_lock_pool_create(pool_t *pool);

#endif

