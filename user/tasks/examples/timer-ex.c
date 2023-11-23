#include <fcntl.h>
#include <sched.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <task.h>
#include <tenok.h>
#include <time.h>
#include <unistd.h>

void timer_callback(union sigval sv)
{
    printf("timer: time's up.\n\r");
}

void timer_task(void)
{
    setprogname("timer-ex");

    timer_t timerid;
    struct sigevent sev;
    struct itimerspec its;

    /* Set up the timer callback function */
    sev.sigev_notify = SIGEV_SIGNAL;
    sev.sigev_notify_function = timer_callback;
    sev.sigev_notify_attributes = NULL;
    sev.sigev_value.sival_ptr = &timerid;

    /* Create a timer */
    if (timer_create(CLOCK_MONOTONIC, &sev, &timerid) == -1) {
        printf("failed to create timer.\n\r");
        exit(1);
    }

    /* Set the timer to expire after 2 seconds
     * and repeats every 2 seconds
     */
    its.it_value.tv_sec = 2;
    its.it_value.tv_nsec = 0;
    its.it_interval.tv_sec = 2;
    its.it_interval.tv_nsec = 0;

    /* Arm the timer */
    if (timer_settime(timerid, 0, &its, NULL) == -1) {
        printf("failed to set the timer.\n\r");
    }

    /* Sleep */
    while (1) {
        sleep(1);
    }
}

HOOK_USER_TASK(timer_task, 0, 1024);
