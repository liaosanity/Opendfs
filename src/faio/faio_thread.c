#include <errno.h>
#include <sys/time.h>

#include "faio_thread.h"
#include "faio_manager.h"

void faio_cond_signal(faio_cond_t *cond, unsigned int limit)
{
    pthread_cond_signal(&cond->wait);
}

void faio_cond_init(faio_cond_t *cond) 
{
    pthread_cond_init(&cond->wait, 0);
    cond->signal = 0;
}

int faio_cond_timedwait(faio_cond_t *cond, faio_mutex_t *mutex, 
    struct timespec *to)
{
    if (pthread_cond_timedwait(&cond->wait, mutex, to) == ETIMEDOUT) 
	{
        return FAIO_COND_TIMEOUT;
    }

    return FAIO_COND_OK;    
}

void faio_cond_wait(faio_cond_t *cond, faio_mutex_t *mutex)
{
    pthread_cond_wait(&cond->wait, mutex);
}

int faio_condition_init(faio_condition_t *cond)
{
    pthread_condattr_t   *cond_attr = NULL;
    pthread_mutexattr_t  *mutex_attr = NULL;

    if (!cond) 
	{
        return FAIO_ERROR;
    }

    if (pthread_mutex_init(&cond->mutex, mutex_attr)) 
	{
        return FAIO_ERROR;
    }
	
    if (pthread_cond_init(&cond->cond, cond_attr)) 
	{
        return FAIO_ERROR;
    }

    cond->signal = FAIO_FALSE;
    
    return FAIO_OK;
}

void faio_condition_destroy(faio_condition_t *cond)
{
    pthread_mutex_destroy(&cond->mutex);
    pthread_cond_destroy(&cond->cond);
}

/* 
 * timeout:millisecond  
 * > 0 : wait for timeout without signal
 * == 0 : don't wait
 * < 0 : wait for signal
 */
int faio_condition_wait(faio_condition_t *cond, int64_t timeout)
{
    struct timespec tp = {0, 0};
    struct timeval now = {0, 0};

    pthread_mutex_lock(&cond->mutex);
	
    while (!cond->signal && timeout) 
	{
        if (timeout > 0) 
		{
            gettimeofday(&now, NULL);
            tp.tv_sec = now.tv_sec + timeout / 1000;
            tp.tv_nsec = (now.tv_usec + (timeout % 1000) * 1000) * 1000;
            pthread_cond_timedwait(&cond->cond, &cond->mutex, &tp);
            timeout = 0;
        } 
		else 
		{
            pthread_cond_wait(&cond->cond, &cond->mutex);
        }
    }
	
    pthread_mutex_unlock(&cond->mutex);
    
    pthread_mutex_lock(&cond->mutex);
	
    cond->signal = FAIO_FALSE;
	
    pthread_mutex_unlock(&cond->mutex);

    return FAIO_OK;
}

int faio_condition_signal(faio_condition_t *cond)
{
    pthread_mutex_lock(&cond->mutex);
	
    if (!cond->signal) 
	{
        pthread_cond_signal(&cond->cond);
        cond->signal = FAIO_TRUE;
    }
	
    pthread_mutex_unlock(&cond->mutex);

    return FAIO_OK;
}

int faio_condition_broadcast(faio_condition_t *cond)
{
    pthread_mutex_lock(&cond->mutex);
	
    if (!cond->signal) 
	{
        pthread_cond_broadcast(&cond->cond);
        cond->signal = FAIO_TRUE;
    }
	
    pthread_mutex_unlock(&cond->mutex);

    return FAIO_OK;
}

