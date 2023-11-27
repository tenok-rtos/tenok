#include <stdint.h>
#include <task.h>
#include <tenok.h>

#include <arch/port.h>
#include <kernel/syscall.h>

NACKED void *thread_info(struct thread_stat *info, void *next)
{
    SYSCALL(THREAD_INFO);
}

NACKED void setprogname(const char *name)
{
    SYSCALL(SETPROGNAME);
}

NACKED int _getpid(void)
{
    SYSCALL(GETPID);
}

NACKED int task_create(task_func_t task_func, uint8_t priority, int stack_size)
{
    SYSCALL(TASK_CREATE);
}
