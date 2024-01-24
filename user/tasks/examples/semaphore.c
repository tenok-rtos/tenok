#include <semaphore.h>
#include <stdio.h>
#include <task.h>
#include <tenok.h>
#include <unistd.h>

sem_t sem_print;

void semaphore_task1(void)
{
    sem_init(&sem_print, 0, 0);

    setprogname("semaphore-1");

    while (1) {
        sleep(1);
        sem_post(&sem_print);
        printf("[semaphore 1] posted semaphore.\n\r");
    }
}

void semaphore_task2(void)
{
    setprogname("semaphore-2");

    while (1) {
        sem_wait(&sem_print);
        printf("[semaphore 2] received semaphore.\n\r");
    }
}

HOOK_USER_TASK(semaphore_task1, 0, STACK_SIZE_MIN);
HOOK_USER_TASK(semaphore_task2, 0, STACK_SIZE_MIN);
