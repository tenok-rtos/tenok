#include <stdint.h>

#include <arch/port.h>
#include <kernel/syscall.h>

NACKED void set_irq(uint32_t state)
{
    SYSCALL(SET_IRQ);
}

NACKED void set_program_name(char *name)
{
    SYSCALL(SET_PROGRAM_NAME);
}

NACKED uint32_t delay_ticks(uint32_t ticks)
{
    SYSCALL(DELAY_TICKS);
}

NACKED void sched_yield(void)
{
    SYSCALL(SCHED_YIELD);
}

NACKED int fork(void)
{
    SYSCALL(FORK);
}

NACKED int getpriority(void)
{
    SYSCALL(GETPRIORITY);
}

NACKED int setpriority(int which, int who, int prio)
{
    SYSCALL(SETPRIORITY);
}

NACKED int getpid(void)
{
    SYSCALL(GETPID);
}
