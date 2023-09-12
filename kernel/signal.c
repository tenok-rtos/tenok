#include <stdbool.h>

#include <arch/port.h>
#include <kernel/syscall.h>

#include <tenok/signal.h>

bool is_signal_defined(int signum)
{
    switch(signum) {
        case SIGUSR1:
        case SIGUSR2:
        case SIGALRM:
        case SIGPOLL:
        case SIGSTOP:
        case SIGTSTP:
        case SIGCONT:
        case SIGINT:
        case SIGKILL:
            return true;
    }

    return false;
}

NACKED int sigaction(int signum, const struct sigaction *act,
                     struct sigaction *oldact)
{
    SYSCALL(SIGACTION);
}

NACKED int kill(pid_t pid, int sig)
{
    SYSCALL(KILL);
}
