#include <errno.h>
#include <string.h>
#include <semaphore.h>

#include <arch/port.h>
#include <kernel/errno.h>
#include <kernel/kernel.h>
#include <kernel/syscall.h>
#include <kernel/semaphore.h>

void sema_init(struct semaphore *sem, int val)
{
    sem->count = val;
    INIT_LIST_HEAD(&sem->wait_list);
}

int down(struct semaphore *sem)
{
    CURRENT_THREAD_INFO(curr_thread);

    if(sem->count <= 0) {
        /* failed to obtain the semaphore, put the current thread into the waiting list */
        prepare_to_wait(&sem->wait_list, &curr_thread->list, THREAD_WAIT);

        return -ERESTARTSYS;
    } else {
        /* successfully obtained the semaphore */
        sem->count--;

        return 0;
    }
}

int down_trylock(struct semaphore *sem)
{
    if(sem->count <= 0) {
        return -EAGAIN;
    } else {
        /* successfully obtained the semaphore */
        sem->count--;

        return 0;
    }
}

int up(struct semaphore *sem)
{
    /* prevent integer overflow */
    if(sem->count >= (INT32_MAX - 1)) {
        return -EOVERFLOW;
    } else {
        /* increase the semaphore */
        sem->count++;

        /* wake up a thread from the waiting list */
        if(sem->count > 0 && !list_empty(&sem->wait_list)) {
            wake_up(&sem->wait_list);
        }

        return 0;
    }
}

int sem_init(sem_t *sem, int pshared, unsigned int value)
{
    if(!sem)
        return -ENOMEM;

    sema_init(sem, value);
    return 0;
}

int sem_destroy(sem_t *sem)
{
    if(!sem)
        return -ENOMEM;

    memset(sem, 0, sizeof(sem_t));
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
