#ifndef __SEMAPHORE_H__
#define __SEMAPHORE_H__

int sem_init(sem_t *sem, int pshared, unsigned int value);
int sem_post(sem_t *sem);
int sem_wait(sem_t *sem);

#endif
