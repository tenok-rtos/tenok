/**
 * @file
 */
#ifndef __SIGNAL_H__
#define __SIGNAL_H__

#include <sys/types.h>

#define SIGUSR1  10
#define SIGUSR2  12
#define SIGPOLL  29
#define SIGSTOP  19
#define SIGCONT  18
#define SIGKILL  9
#define SIGNAL_CNT 6

#define SA_SIGINFO 0x2

#define SIGEV_NONE   1
#define SIGEV_SIGNAL 2

typedef	uint32_t sigset_t;

union sigval {
    int   sival_int;
    void *sival_ptr;
};

typedef struct {
    int          si_signo;
    int          si_code;
    union sigval si_value;
} siginfo_t;

struct sigaction {
    union {
        void (*sa_handler)(int);
        void (*sa_sigaction)(int, siginfo_t *, void *);
    };
    sigset_t sa_mask;
    int      sa_flags;
};

typedef void (*sa_handler_t)(int);
typedef void (*sa_sigaction_t)(int, siginfo_t *, void *);

struct sigevent {
    int   sigev_notify;
    int   sigev_signo;
    union sigval sigev_value;
    void  (*sigev_notify_function)(union sigval);
    void  *sigev_notify_attributes;
    pid_t sigev_notify_thread_id;
};

/**
 * @brief  Initialize the signal set given by set  to  empty, with all signals
           excluded from the set
 * @param  set: The signal set object to provide.
 * @retval int: 0 on sucess and nonzero error number on error.
 */
int sigemptyset(sigset_t *set);

/**
 * @brief  Initialize set to full, including all signals
 * @param  set: The signal set object to provide.
 * @retval int: 0 on sucess and nonzero error number on error.
 */
int sigfillset(sigset_t *set);

/**
 * @brief  Add signal signum to set
 * @param  set: The signal set object to provide.
 * @param  signum: The signal number to provide.
 * @retval int: 0 on sucess and nonzero error number on error.
 */
int sigaddset(sigset_t *set, int signum);

/**
 * @brief  Delete signal sugnum from set
 * @param  set: The signal set object to provide.
 * @param  signum: The signal number to provide.
 * @retval int: 0 on sucess and nonzero error number on error.
 */
int sigdelset(sigset_t *set, int signum);

/**
 * @brief  Signal a task to change the action taken by a task on receipt of a specific
           signal
 * @param  signum: The signal set object to provide.
 * @param  act: The new action to provide.
 * @param  oldact: For preserving old action.
 * @retval int: 0 on sucess and nonzero error number on error.
 */
int sigaction(int signum, const struct sigaction *act,
              struct sigaction *oldact);

/**
 * @brief  Suspend execution of the calling thread until one of the signals specified
           in the signal set becomes pending
 * @param  set: The signal set object to provide.
 * @param  signum: The signal number to provide.
 * @retval int: 0 on sucess and nonzero error number on error.
 */
int sigwait(const sigset_t *set, int *sig);

/**
 * @brief  Cause the calling task (or thread) to sleep until a signal is delivered
           that either terminate the task or cause the invocation of a signal-catching
           function
 * @retval int: 0 on sucess and nonzero error number on error.
 */
int pause(void);

/**
 * @brief  Send a signal sig to the task specified with pid
 * @param  pid: The task ID to provide.
 * @param  signum: The signal number to provide.
 * @retval int: 0 on sucess and nonzero error number on error.
 */
int kill(pid_t pid, int sig);

#endif
