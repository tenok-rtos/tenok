#include <fcntl.h>
#include <sched.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <task.h>
#include <tenok.h>
#include <unistd.h>

static int task1_pid;
static int serial_fd;

void sig_handler(int signum)
{
    dprintf(serial_fd, "received signal %d.\n\r", signum);
}

void signal_task1(void)
{
    task1_pid = getpid();

    setprogname("signal-ex-1");

    serial_fd = open("/dev/serial0", O_RDWR);
    if (serial_fd < 0) {
        exit(1);
    }

    /* register the signal handler function */
    struct sigaction sa;
    sigemptyset(&sa.sa_mask);
    sa.sa_handler = sig_handler;
    sa.sa_flags = 0;
    sigaction(SIGUSR1, &sa, NULL);

    /* block until the signal arrived */
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGUSR1);
    int sig;
    sigwait(&set, &sig);

    /* print after the signal arrived */
    dprintf(serial_fd, "signal %d is captured.\n\r", sig);

    /* sleep */
    while (1) {
        sleep(1);
    }
}

void signal_task2(void)
{
    setprogname("signal-ex-2");

    sleep(1);

    /* send the signal */
    kill(task1_pid, SIGUSR1);

    /* sleep */
    while (1) {
        sleep(1);
    }
}

HOOK_USER_TASK(signal_task1, 0, STACK_SIZE_MIN);
HOOK_USER_TASK(signal_task2, 0, STACK_SIZE_MIN);
