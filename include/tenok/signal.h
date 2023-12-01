/**
 * @file
 */
#ifndef __SIGNAL_H__
#define __SIGNAL_H__

#include <sys/types.h>

#define SIGUSR1 10
#define SIGUSR2 12
#define SIGPOLL 29
#define SIGSTOP 19
#define SIGCONT 18
#define SIGKILL 9
#define SIGNAL_CNT 6

#define SA_SIGINFO 0x2

#define SIGEV_NONE 1
#define SIGEV_SIGNAL 2

typedef uint32_t sigset_t;

union sigval {
    int sival_int;
    void *sival_ptr;
};

typedef struct {
    int si_signo;
    int si_code;
    union sigval si_value;
} siginfo_t;

struct sigaction {
    union {
        void (*sa_handler)(int);
        void (*sa_sigaction)(int, siginfo_t *, void *);
    };
    sigset_t sa_mask;
    int sa_flags;
};

typedef void (*sa_handler_t)(int);
typedef void (*sa_sigaction_t)(int, siginfo_t *, void *);

struct sigevent {
    int sigev_notify;
    int sigev_signo;
    union sigval sigev_value;
    void (*sigev_notify_function)(union sigval);
    void *sigev_notify_attributes;
    pid_t sigev_notify_thread_id;
};

/**
 * @brief  Initialize the signal set given by set to empty, with all signals
 *         excluded from the set
 * @param  set: Pointer to the signal set.
 * @retval int: 0 on success and nonzero error number on error.
 */
int sigemptyset(sigset_t *set);

/**
 * @brief  Initialize the signal set to full, including all signals
 * @param  set: Pointer to the signal set.
 * @retval int: 0 on success and nonzero error number on error.
 */
int sigfillset(sigset_t *set);

/**
 * @brief  Add a signal into the set
 * @param  set: Pointer to the signal set.
 * @param  signum: The number of the signal to add into the set.
 * @retval int: 0 on success and nonzero error number on error.
 */
int sigaddset(sigset_t *set, int signum);

/**
 * @brief  Delete a signal from the set
 * @param  set: Pointer to the signal set
 * @param  signum: The number of the signal to delete from the set
 * @retval int: 0 on success and nonzero error number on error.
 */
int sigdelset(sigset_t *set, int signum);

/**
 * @brief  Test whether sugnum is a member of the set
 * @param  set: Pointer to the signal set.
 * @param  signum: The number of the signal to check.
 * @retval int: 0 on success and nonzero error number on error.
 */
int sigismember(const sigset_t *set, int signum);

/**
 * @brief  Set up a signal for the task to catch.
 * @param  signum: The number of the signal to attach.
 * @param  act: Pointer of the new action setting.
 * @param  oldact: For preserving old action.
 * @retval int: 0 on success and nonzero error number on error.
 */
int sigaction(int signum,
              const struct sigaction *act,
              struct sigaction *oldact);

/**
 * @brief  Suspend execution of the calling task until one of the signals
 *         specified in the signal set becomes pending
 * @param  set: Pointer to the signal set.
 * @param  sig: For returning the caught signal.
 * @retval int: 0 on success and nonzero error number on error.
 */
int sigwait(const sigset_t *set, int *sig);

/**
 * @brief  To cause the calling task (or thread) to sleep until a signal is
 *         delivered that either terminate the task or cause the invocation
 *         of a signal-catching function
 * @retval int: 0 on success and nonzero error number on error.
 */
int pause(void);

/**
 * @brief  Send a signal to a task
 * @param  pid: The task ID to provide.
 * @param  sig: The signal number to provide.
 * @retval int: 0 on success and nonzero error number on error.
 */
int kill(pid_t pid, int sig);

/**
 * @brief  Send a signal to the calling task
 * @param  sig: The signal number to provide.
 * @retval int: 0 on success and nonzero error number on error.
 */
int raise(int sig);

#endif
