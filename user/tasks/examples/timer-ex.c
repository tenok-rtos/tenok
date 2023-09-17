#include <string.h>

#include <kernel/task.h>

#include <tenok/tenok.h>
#include <tenok/time.h>
#include <tenok/fcntl.h>
#include <tenok/sched.h>
#include <tenok/signal.h>
#include <tenok/unistd.h>

#define print(fd, str) write(fd, str, strlen(str))

void timer_callback(union sigval sv)
{
    //TODO: print message
}

void timer_task(void)
{
    setprogname("timer");

    int serial_fd = open("/dev/serial0", 0);

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
        print(serial_fd, "failed to create timer.\n\r");
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
        print(serial_fd, "failed to set the timer.\n\r");
    }

    /* sleep forever */
    sched_yield();

    while(1);
}

HOOK_USER_TASK(timer_task);
