#ifndef __SEMAPHORE_H__
#define __SEMAPHORE_H__

#include "spinlock.h"

typedef struct {
	spinlock_t lock;
	int32_t    count;
	list_t     wait_list;
} sem_t;

int sem_init(sem_t *sem, int pshared, unsigned int value);
int sem_post(sem_t *sem);
int sem_wait(sem_t *sem);

#endif
