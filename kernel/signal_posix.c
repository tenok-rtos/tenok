#include <errno.h>
#include <signal.h>
#include <stddef.h>

/* The older way of asking for a handler, defined by sigaction() */
sa_handler_t signal(int signum, sa_handler_t handler)
{
    struct sigaction act = {0}, old = {0};

    act.sa_handler = handler;
    sigemptyset(&act.sa_mask);

    if (sigaction(signum, &act, &old) != 0)
        return SIG_ERR;

    return old.sa_handler;
}

/* Tenok blocks no signal, so there is no mask of its own to keep */
int sigprocmask(int how, const sigset_t *set, sigset_t *oldset)
{
    if (oldset)
        *oldset = 0;

    return 0;
}

/* Nothing blocks a signal here, so there is never one to wait for */
int sigsuspend(const sigset_t *mask)
{
    errno = EINTR;

    return -1;
}
