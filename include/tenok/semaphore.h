#ifndef __SEMAPHORE_H__
#define __SEMAPHORE_H__

#include "kernel/semaphore.h"

int sem_init(sem_t *sem, int pshared, unsigned int value);
int sem_post(sem_t *sem);
int sem_trywait(sem_t *sem);
int sem_wait(sem_t *sem);
int sem_getvalue(sem_t *sem, int *sval);

#endif
