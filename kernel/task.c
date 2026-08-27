#include <errno.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdint.h>
#include <string.h>
#include <sys/prctl.h>
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

/* Every task of Tenok comes from the image the system booted, and nothing can
 * make another one from a running task
 */
pid_t vfork(void)
{
    errno = ENOSYS;
    return -1;
}

pid_t fork(void)
{
    errno = ENOSYS;
    return -1;
}

/* A session gathers the tasks a terminal speaks to. Tenok has the one console
 * and nothing to gather
 */
pid_t setsid(void)
{
    errno = ENOSYS;
    return -1;
}

/* A program of Tenok is linked into the firmware and reached by calling it.
 * No file the file system holds is in a format the system can execute, which
 * is what POSIX has ENOEXEC say
 */
int execve(const char *pathname, char *const argv[], char *const envp[])
{
    errno = ENOEXEC;
    return -1;
}

int execvp(const char *file, char *const argv[])
{
    errno = ENOEXEC;
    return -1;
}

/* The name is the whole of what a thread of Tenok carries to be asked about,
 * so the two options that ask about it are the two this answers
 */
int prctl(int option, ...)
{
    va_list ap;
    va_start(ap, option);
    char *name = va_arg(ap, char *);
    va_end(ap);

    if (!name) {
        errno = EFAULT;
        return -1;
    }

    switch (option) {
    case PR_SET_NAME:
        setprogname(name);
        return 0;

    case PR_GET_NAME: {
        /* The caller says nothing about the room it hands over, so what is
         * written is the longest name a thread of Tenok can be given
         */
        pthread_t self = pthread_self();
        struct thread_stat info;
        void *next = NULL;

        while ((next = thread_info(&info, next))) {
            if ((pthread_t) info.tid != self)
                continue;

            strncpy(name, info.name, PR_NAME_MAX - 1);
            name[PR_NAME_MAX - 1] = '\0';
            return 0;
        }

        errno = ESRCH;
        return -1;
    }

    default:
        errno = EINVAL;
        return -1;
    }
}

/* A thread is known by the same number either way it is asked for */
pid_t gettid(void)
{
    return (pid_t) pthread_self();
}

NACKED int task_create(task_func_t task_func, uint8_t priority, int stack_size)
{
    SYSCALL(TASK_CREATE);
}
