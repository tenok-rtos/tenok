/**
 * @file
 */
#ifndef __KERNEL_SEMAPHORE_H__
#define __KERNEL_SEMAPHORE_H__

struct semaphore {
    int32_t count;
    struct list wait_list;
};

void sema_init(struct semaphore *sem, int val);

int down(struct semaphore *sem);

int down_trylock(struct semaphore *sem);

int up(struct semaphore *sem);

#endif
