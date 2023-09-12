#include <arch/port.h>
#include <kernel/syscall.h>

#include <tenok/signal.h>

NACKED int sigaction(int signum, const struct sigaction *act,
                     struct sigaction *oldact)
{
    SYSCALL(SIGACTION);
}
