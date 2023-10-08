#include <stdbool.h>
#include <errno.h>

#include <arch/port.h>
#include <kernel/syscall.h>

#include <tenok/signal.h>

#define DEF_SIG_BIT(name, bit) \
    case name: return (1 << bit)

bool is_signal_defined(int signum)
{
    switch(signum) {
        case SIGUSR1:
        case SIGUSR2:
        case SIGPOLL:
        case SIGSTOP:
        case SIGCONT:
        case SIGKILL:
            return true;
    }

    return false;
}

uint32_t sig2bit(int signum)
{
    switch(signum) {
            DEF_SIG_BIT(SIGUSR1, 0);
            DEF_SIG_BIT(SIGUSR2, 1);
            DEF_SIG_BIT(SIGPOLL, 2);
            DEF_SIG_BIT(SIGSTOP, 3);
            DEF_SIG_BIT(SIGCONT, 4);
            DEF_SIG_BIT(SIGKILL, 5);
        default:
            return 0; /* should not happened */
    }
}

int get_signal_index(int signum)
{
    switch(signum) {
        case SIGUSR1:
            return 0;
        case SIGUSR2:
            return 1;
        case SIGPOLL:
            return 2;
        case SIGSTOP:
            return 3;
        case SIGCONT:
            return 4;
        case SIGKILL:
            return 5;
    }

    return 0; /* should not happened */
}

int sigemptyset(sigset_t *set)
{
    *set = 0;
    return 0;
}

int sigfillset(sigset_t *set)
{
    *set = 0xffffffff;
    return 0;
}

int sigaddset(sigset_t *set, int signum)
{
    if(!is_signal_defined(signum)) {
        return -EINVAL;
    }

    *set |= sig2bit(signum);
    return 0;
}

int sigdelset(sigset_t *set, int signum)
{
    if(!is_signal_defined(signum)) {
        return -EINVAL;
    }

    *set &= sig2bit(signum);
    return 0;
}

NACKED int sigaction(int signum, const struct sigaction *act,
                     struct sigaction *oldact)
{
    SYSCALL(SIGACTION);
}

NACKED int sigwait(const sigset_t *set, int *sig)
{
    SYSCALL(SIGWAIT);
}

int pause(void)
{
    int sig;
    sigset_t set = sig2bit(SIGUSR1) | sig2bit(SIGUSR2) |
                   sig2bit(SIGPOLL) | sig2bit(SIGSTOP) |
                   sig2bit(SIGCONT) | sig2bit(SIGKILL);
    sigwait(&set, &sig);
    return 0;
}

NACKED int _kill(pid_t pid, int sig)
{
    SYSCALL(KILL);
}

int kill(pid_t pid, int sig)
{
    return _kill(pid, sig);
}
