#include <tenok.h>
#include <stdint.h>
#include <sys/types.h>

#include <arch/port.h>
#include <kernel/kernel.h>
#include <kernel/syscall.h>

#include "kconfig.h"

NACKED int procstat(struct procstat_info info[TASK_CNT_MAX])
{
    SYSCALL(PROCSTAT);
}

NACKED void setprogname(const char *name)
{
    SYSCALL(SETPROGNAME);
}

NACKED const char *getprogname(void)
{
    SYSCALL(GETPROGNAME);
}

NACKED int _getpid(void)
{
    SYSCALL(GETPID);
}

NACKED int gettid(void)
{
    SYSCALL(GETTID);
}

NACKED int task_create(task_func_t task_func, uint8_t priority, int stack_size)
{
    SYSCALL(TASK_CREATE);
}
