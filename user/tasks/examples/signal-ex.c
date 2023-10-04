#include <tenok.h>
#include <string.h>
#include <stdio.h>
#include <sched.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>

#include <kernel/task.h>

static int task1_pid;
static int serial_fd;

void sig_handler(int signum)
{
    char s[100] = {0};
    sprintf(s, "received signal %d.\n\r", signum);
    write(serial_fd, s, strlen(s));
}

void signal_task1(void)
{
    task1_pid = getpid();

    setprogname("signal1");

    serial_fd = open("/dev/serial0", 0);

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
    char s[100] = {0};
    sprintf(s, "signal %d is captured.\n\r", sig);
    write(serial_fd, s, strlen(s));

    /* sleep */
    while(1) {
        sleep(1);
    }
}

void signal_task2(void)
{
    setprogname("signal2");

    sleep(1);

    /* send the signal */
    kill(task1_pid, SIGUSR1);

    /* sleep */
    while(1) {
        sleep(1);
    }
}

HOOK_USER_TASK(signal_task1, 0, 2048);
HOOK_USER_TASK(signal_task2, 0, 2048);
