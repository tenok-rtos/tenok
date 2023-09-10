#ifndef __PTHREAD_H__
#define __PTHREAD_H__

#include "mutex.h"
#include "cond.h"

int pthread_cond_init(pthread_cond_t *cond, pthread_condattr_t cond_attr);
int pthread_cond_signal(pthread_cond_t *cond);
int pthread_cond_wait(pthread_cond_t *cond, pthread_mutex_t *mutex);
int pthread_mutex_init(pthread_mutex_t *mutex, const pthread_mutex_attr_t *attr);
int pthread_mutex_unlock(pthread_mutex_t *mutex);
int pthread_mutex_lock(pthread_mutex_t *mutex);

#endif
