/**
 * @file
 * What the stack asks of the system under it, answered with what this kernel
 * offers a thread.
 *
 * Everything here is a thin turn of one name into another. The one thing the
 * kernel does not already have is a post box -- a queue of pointers a thread
 * can wait on -- so that is built here out of a ring, a lock and two counts.
 */

#include <errno.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "arch/sys_arch.h"
#include "lwip/err.h"
#include "lwip/sys.h"

/*=========*
 * Clocks  *
 *=========*/

u32_t sys_now(void)
{
    struct timespec tp;

    clock_gettime(CLOCK_MONOTONIC, &tp);

    return (u32_t) (tp.tv_sec * 1000 + tp.tv_nsec / 1000000);
}

u32_t sys_jiffies(void)
{
    return sys_now();
}

void sys_init(void)
{
}

/* What the stack waits on is an instant in the future, and what the kernel
 * waits on is an instant on the clock, so the one is turned into the other
 */
static void deadline_after(struct timespec *ts, u32_t ms)
{
    clock_gettime(CLOCK_MONOTONIC, ts);

    ts->tv_sec += ms / 1000;
    ts->tv_nsec += (long) (ms % 1000) * 1000000L;

    if (ts->tv_nsec >= 1000000000L) {
        ts->tv_sec++;
        ts->tv_nsec -= 1000000000L;
    }
}

/*==========*
 * Mutexes  *
 *==========*/

err_t sys_mutex_new(sys_mutex_t *mutex)
{
    if (pthread_mutex_init(&mutex->mtx, NULL) != 0)
        return ERR_MEM;

    mutex->valid = 1;

    return ERR_OK;
}

void sys_mutex_lock(sys_mutex_t *mutex)
{
    pthread_mutex_lock(&mutex->mtx);
}

void sys_mutex_unlock(sys_mutex_t *mutex)
{
    pthread_mutex_unlock(&mutex->mtx);
}

void sys_mutex_free(sys_mutex_t *mutex)
{
    pthread_mutex_destroy(&mutex->mtx);
    mutex->valid = 0;
}

int sys_mutex_valid(sys_mutex_t *mutex)
{
    return mutex->valid;
}

void sys_mutex_set_invalid(sys_mutex_t *mutex)
{
    mutex->valid = 0;
}

/*=============*
 * Semaphores  *
 *=============*/

err_t sys_sem_new(sys_sem_t *sem, u8_t count)
{
    if (sem_init(&sem->sem, 0, count) != 0)
        return ERR_MEM;

    sem->valid = 1;

    return ERR_OK;
}

void sys_sem_signal(sys_sem_t *sem)
{
    sem_post(&sem->sem);
}

/* The stack wants to be told how long it waited, and to be told a particular
 * value when it waited in vain. A timeout of zero means it will wait however
 * long it takes
 */
u32_t sys_arch_sem_wait(sys_sem_t *sem, u32_t timeout)
{
    u32_t start = sys_now();

    if (timeout == 0) {
        while (sem_wait(&sem->sem) != 0)
            ;
        return sys_now() - start;
    }

    struct timespec ts;
    deadline_after(&ts, timeout);

    if (sem_timedwait(&sem->sem, &ts) != 0)
        return SYS_ARCH_TIMEOUT;

    return sys_now() - start;
}

void sys_sem_free(sys_sem_t *sem)
{
    sem_destroy(&sem->sem);
    sem->valid = 0;
}

int sys_sem_valid(sys_sem_t *sem)
{
    return sem->valid;
}

void sys_sem_set_invalid(sys_sem_t *sem)
{
    sem->valid = 0;
}

/*============*
 * Post boxes *
 *============*/

err_t sys_mbox_new(sys_mbox_t *mbox, int size)
{
    if (size <= 0 || size > SYS_MBOX_MAX)
        size = SYS_MBOX_MAX;

    memset(mbox, 0, sizeof(*mbox));

    mbox->size = size;

    if (pthread_mutex_init(&mbox->mtx, NULL) != 0)
        return ERR_MEM;

    /* One count says how much is in it, the other how much room is left */
    if (sem_init(&mbox->used, 0, 0) != 0)
        return ERR_MEM;

    if (sem_init(&mbox->free, 0, size) != 0)
        return ERR_MEM;

    mbox->valid = 1;

    return ERR_OK;
}

static void mbox_put(sys_mbox_t *mbox, void *msg)
{
    pthread_mutex_lock(&mbox->mtx);
    mbox->msg[mbox->head] = msg;
    mbox->head = (mbox->head + 1) % mbox->size;
    pthread_mutex_unlock(&mbox->mtx);

    sem_post(&mbox->used);
}

static void *mbox_get(sys_mbox_t *mbox)
{
    pthread_mutex_lock(&mbox->mtx);
    void *msg = mbox->msg[mbox->tail];
    mbox->tail = (mbox->tail + 1) % mbox->size;
    pthread_mutex_unlock(&mbox->mtx);

    sem_post(&mbox->free);

    return msg;
}

void sys_mbox_post(sys_mbox_t *mbox, void *msg)
{
    while (sem_wait(&mbox->free) != 0)
        ;

    mbox_put(mbox, msg);
}

err_t sys_mbox_trypost(sys_mbox_t *mbox, void *msg)
{
    if (sem_trywait(&mbox->free) != 0)
        return ERR_MEM;

    mbox_put(mbox, msg);

    return ERR_OK;
}

err_t sys_mbox_trypost_fromisr(sys_mbox_t *mbox, void *msg)
{
    return sys_mbox_trypost(mbox, msg);
}

u32_t sys_arch_mbox_fetch(sys_mbox_t *mbox, void **msg, u32_t timeout)
{
    u32_t start = sys_now();

    if (timeout == 0) {
        while (sem_wait(&mbox->used) != 0)
            ;
    } else {
        struct timespec ts;
        deadline_after(&ts, timeout);

        if (sem_timedwait(&mbox->used, &ts) != 0)
            return SYS_ARCH_TIMEOUT;
    }

    void *got = mbox_get(mbox);
    if (msg)
        *msg = got;

    return sys_now() - start;
}

u32_t sys_arch_mbox_tryfetch(sys_mbox_t *mbox, void **msg)
{
    if (sem_trywait(&mbox->used) != 0)
        return SYS_MBOX_EMPTY;

    void *got = mbox_get(mbox);
    if (msg)
        *msg = got;

    return 0;
}

void sys_mbox_free(sys_mbox_t *mbox)
{
    sem_destroy(&mbox->used);
    sem_destroy(&mbox->free);
    pthread_mutex_destroy(&mbox->mtx);
    mbox->valid = 0;
}

int sys_mbox_valid(sys_mbox_t *mbox)
{
    return mbox->valid;
}

void sys_mbox_set_invalid(sys_mbox_t *mbox)
{
    mbox->valid = 0;
}

/*==========*
 * Threads  *
 *==========*/

sys_thread_t sys_thread_new(const char *name,
                            lwip_thread_fn thread,
                            void *arg,
                            int stacksize,
                            int prio)
{
    pthread_attr_t attr;
    pthread_t tid;

    pthread_attr_init(&attr);
    pthread_attr_setstacksize(&attr, stacksize);

    struct sched_param param = {.sched_priority = prio};
    pthread_attr_setschedparam(&attr, &param);

    if (pthread_create(&tid, &attr, (void *(*) (void *) ) thread, arg) != 0)
        return 0;

    return (sys_thread_t) tid;
}
