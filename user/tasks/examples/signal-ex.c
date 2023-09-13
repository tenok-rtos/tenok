#include <kernel/task.h>

#include <tenok/sched.h>
#include <tenok/signal.h>
#include <tenok/unistd.h>

void sig_handler(int signum)
{
    //TODO: print message
}

void signal_task(void)
{
    set_program_name("signal");

    struct sigaction sa;

    /* set up the handler function for SIGINT */
    sa.sa_handler = sig_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    /* register the signal */
    sigaction(SIGUSR1, &sa, NULL);

    /* send the signal for testing */
    kill(getpid(), SIGUSR1);

    /* sleep forever */
    sched_yield();

    while(1);
}

HOOK_USER_TASK(signal_task);
