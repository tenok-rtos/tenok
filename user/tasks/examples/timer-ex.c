#include <tenok.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>
#include <sched.h>
#include <signal.h>
#include <unistd.h>

#include <kernel/task.h>

static int serial_fd;

void timer_callback(union sigval sv)
{
    dprintf(serial_fd, "timer: time's up.\n\r");
}

void timer_task(void)
{
    setprogname("timer");

    serial_fd = open("/dev/serial0", O_RDWR);
    if(serial_fd < 0) {
        exit(1);
    }

    timer_t timerid;
    struct sigevent sev;
    struct itimerspec its;

    /* set up the timer callback function */
    sev.sigev_notify = SIGEV_SIGNAL;
    sev.sigev_notify_function = timer_callback;
    sev.sigev_notify_attributes = NULL;
    sev.sigev_value.sival_ptr = &timerid;

    /* create a timer */
    if(timer_create(CLOCK_MONOTONIC, &sev, &timerid) == -1) {
        dprintf(serial_fd, "failed to create timer.\n\r");
        exit(1);
    }

    /* set the timer to expire after 2 seconds
     * and repeat every 2 seconds
     */
    its.it_value.tv_sec = 2;
    its.it_value.tv_nsec = 0;
    its.it_interval.tv_sec = 2;
    its.it_interval.tv_nsec = 0;

    /* arm the timer */
    if(timer_settime(timerid, 0, &its, NULL) == -1) {
        dprintf(serial_fd, "failed to set the timer.\n\r");
    }

    /* sleep */
    while(1) {
        sleep(1);
    }
}

HOOK_USER_TASK(timer_task, 0, 1024);
