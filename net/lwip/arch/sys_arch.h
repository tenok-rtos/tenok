/**
 * @file
 * The shapes of the things the stack waits on, which are the kernel's own
 * with a mark beside each saying whether it has been made yet.
 */
#ifndef __LWIP_ARCH_SYS_ARCH_H__
#define __LWIP_ARCH_SYS_ARCH_H__

#include <pthread.h>
#include <semaphore.h>
#include <stdint.h>

#define SYS_MBOX_NULL NULL
#define SYS_SEM_NULL NULL
#define SYS_MBOX_MAX 16

typedef struct {
    pthread_mutex_t mtx;
    int valid;
} sys_mutex_t;

typedef struct {
    sem_t sem;
    int valid;
} sys_sem_t;

typedef struct {
    void *msg[SYS_MBOX_MAX];
    int head;
    int tail;
    int size;
    pthread_mutex_t mtx;
    sem_t used;
    sem_t free;
    int valid;
} sys_mbox_t;

typedef uintptr_t sys_thread_t;
typedef int sys_prot_t;

#endif
