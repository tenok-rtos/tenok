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

static NACKED pid_t __getpid(void)
{
    SYSCALL(GETPID);
}

pid_t getpid(void)
{
    return __getpid();
}

int _getpid(void)
{
    return getpid();
}

/* No task of Tenok is created by another, the kernel starts them all. Unix
 * numbers the scheduler zero and gives it as the parent of what has none, and
 * zero is the idle task of Tenok for the same reason
 */
pid_t getppid(void)
{
    return 0;
}

NACKED int task_create(task_func_t task_func, uint8_t priority, int stack_size)
{
    SYSCALL(TASK_CREATE);
}
