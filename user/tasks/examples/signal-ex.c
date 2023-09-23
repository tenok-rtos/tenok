#include <string.h>

#include <kernel/task.h>

#include <tenok/tenok.h>
#include <tenok/stdio.h>
#include <tenok/sched.h>
#include <tenok/signal.h>
#include <tenok/unistd.h>
#include <tenok/fcntl.h>

int serial_fd;

void sig_handler(int signum)
{
    char s[100] = {0};
    sprintf(s, "received signal %d.\n\r", signum);
    write(serial_fd, s, strlen(s));
}

void signal_task(void)
{
    setprogname("signal");

    serial_fd = open("/dev/serial0", 0);

    struct sigaction sa;

    /* set up the handler function for SIGINT */
    sa.sa_handler = sig_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    /* register the signal */
    sigaction(SIGUSR1, &sa, NULL);

    /* send the signal for testing */
    kill(getpid(), SIGUSR1);

    /* sleep */
    while(1) {
        sleep(1);
    };
}

HOOK_USER_TASK(signal_task);
