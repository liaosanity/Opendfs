#ifndef FAIO_THREAD_H
#define FAIO_THREAD_H

#include <stdint.h>
#include <pthread.h>

#define FAIO_COND_OK            0
#define FAIO_COND_TIMEOUT       1

typedef struct faio_cond_s 
{
    pthread_cond_t      wait;
    unsigned int        signal;
} faio_cond_t;

typedef pthread_mutex_t faio_mutex_t;
typedef pthread_t       faio_worker_t;

typedef struct faio_condition_s 
{
    pthread_mutex_t         mutex;
    pthread_cond_t          cond;
    int                     signal;
} faio_condition_t;

#define faio_mutex_init(mutex)	    pthread_mutex_init(&(mutex), 0)
#define faio_mutex_destroy(mutex)   pthread_mutex_destroy(&(mutex))

#define faio_lock(mutex)		        pthread_mutex_lock(&(mutex))
#define faio_unlock(mutex)		        pthread_mutex_unlock(&(mutex))
#define faio_cond_destroy(cond)         pthread_cond_destroy(&((cond).wait))

void faio_cond_signal(faio_cond_t *cond, unsigned int limit);
void faio_cond_init(faio_cond_t *cond);
int faio_cond_timedwait(faio_cond_t *cond, faio_mutex_t *mutex, 
	struct timespec *to);
void faio_cond_wait(faio_cond_t *cond, faio_mutex_t *mutex);
int faio_condition_init(faio_condition_t *cond);
void faio_condition_destroy(faio_condition_t *cond);
int faio_condition_wait(faio_condition_t *cond, int64_t timeout);
int faio_condition_signal(faio_condition_t *cond);
int faio_condition_broadcast(faio_condition_t *cond);

#endif

