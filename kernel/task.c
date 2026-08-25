#include <errno.h>
#include <stdint.h>
#include <sys/wait.h>
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

/* A task is never the child of another, so a caller of these never has one to
 * wait for. That is the answer POSIX gives when the children have run out, and
 * on Tenok it is the answer from the start
 */
int waitpid(int pid, int *status, int options)
{
    errno = ECHILD;
    return -1;
}

int wait(int *status)
{
    return waitpid(-1, status, 0);
}

NACKED int task_create(task_func_t task_func, uint8_t priority, int stack_size)
{
    SYSCALL(TASK_CREATE);
}
