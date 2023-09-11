#include <arch/port.h>
#include <kernel/syscall.h>

#include <tenok/semaphore.h>

int sem_init(sem_t *sem, int pshared, unsigned int value)
{
    sem->count = value;
    sem->lock = 0;
    list_init(&sem->wait_list);

    return 0;
}

NACKED int sem_post(sem_t *sem)
{
    SYSCALL(SEM_POST);
}

NACKED int sem_trywait(sem_t *sem)
{
    SYSCALL(SEM_TRYWAIT);
}

NACKED int sem_wait(sem_t *sem)
{
    SYSCALL(SEM_WAIT);
}

NACKED int sem_getvalue(sem_t *sem, int *sval)
{
    SYSCALL(SEM_GETVALUE);
}
