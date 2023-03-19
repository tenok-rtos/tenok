#ifndef __SEMAPHORE_H__
#define __SEMAPHORE_H__

#include "spinlock.h"
#include "list.h"

typedef struct {
    spinlock_t lock;
    int32_t    count;
    struct list wait_list;
} sem_t;

int sem_init(sem_t *sem, int pshared, unsigned int value);
int sem_post(sem_t *sem);
int sem_trywait(sem_t *sem);
int sem_wait(sem_t *sem);
int sem_getvalue(sem_t *sem, int *sval);

#endif
