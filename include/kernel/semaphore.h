#ifndef __SEMAPHORE_H__
#define __SEMAPHORE_H__

#include "spinlock.h"
#include "list.h"

struct semaphore {
    spinlock_t lock;
    int32_t    count;
    struct list wait_list;
};

typedef struct semaphore sem_t;

int sema_init(sem_t *sem, unsigned int value);
int up(struct semaphore *sem);
int down_trylock(struct semaphore *sem);
int down(struct semaphore *sem);

#endif
