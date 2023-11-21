#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <task.h>
#include <tenok.h>
#include <time.h>
#include <unistd.h>

static pthread_mutex_t mutex;

void mutex_task_high(void)
{
    setprogname("pri-inv-high");

    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
#if 1
    /* Enable priority inheritance */
    pthread_mutexattr_setprotocol(&attr, PTHREAD_PRIO_INHERIT);
#else
    /* Disable priority inheritance */
    pthread_mutexattr_setprotocol(&attr, PTHREAD_PRIO_NONE);
#endif
    pthread_mutex_init(&mutex, &attr);

    int serial_fd = open("/dev/serial0", O_RDWR);
    if (serial_fd < 0) {
        exit(1);
    }

    dprintf(serial_fd,
            "[mutex high] attempt to lock the mutex in 5 seconds.\n\r");

    sleep(5);

    while (1) {
        /* Start the critical section */
        pthread_mutex_lock(&mutex);

        dprintf(serial_fd,
                "[mutex high] highest-priority thread locked the mutex "
                "successfully.\n\r");

        /* End the critical section */
        pthread_mutex_unlock(&mutex);

        /* Simulate some work */
        sleep(2);
    }
}

void mutex_task_median(void)
{
    setprogname("pri-inv-median");

    struct timespec start_time, curr_time;

    int serial_fd = open("/dev/serial0", O_RDWR);
    if (serial_fd < 0) {
        exit(1);
    }

    /* Occupy the CPU to block the lowest-prioiry thread after 3 seconds */
    dprintf(
        serial_fd,
        "[mutex median] block the lowest-priority thread in 3 seconds.\n\r");
    sleep(3);

    /* Occupy the CPU for 10 seconds */
    dprintf(
        serial_fd,
        "[mutex median] block the lowest-priority thread for 10 seconds.\n\r");

    clock_gettime(CLOCK_MONOTONIC, &start_time);

    while (1) {
        clock_gettime(CLOCK_MONOTONIC, &curr_time);

        if ((curr_time.tv_sec - start_time.tv_sec) > 10)
            break;
    }

    /* Pause the thread after the job is finish */
    while (1) {
        pause();
    }
}

void mutex_task_low(void)
{
    setprogname("pri-inv-low");

    int serial_fd = open("/dev/serial0", O_RDWR);
    if (serial_fd < 0) {
        exit(1);
    }

    while (1) {
        /* Start the critical section */
        pthread_mutex_lock(&mutex);

        dprintf(serial_fd,
                "[mutex low] lowest-priority thread locked the mutex "
                "successfully.\n\r");

        /* Simulate some works */
        sleep(2);

        /* End the critical section */
        pthread_mutex_unlock(&mutex);
    }
}

HOOK_USER_TASK(mutex_task_high, 2, STACK_SIZE_MIN);
HOOK_USER_TASK(mutex_task_median, 1, STACK_SIZE_MIN);
HOOK_USER_TASK(mutex_task_low, 0, STACK_SIZE_MIN);
