/**
 * @file
 */
#ifndef __SIGNAL_H__
#define __SIGNAL_H__

#include <sys/types.h>

struct timespec;

#define SIGUSR1 10
#define SIGUSR2 12
#define SIGPOLL 29
#define SIGSTOP 19
#define SIGCONT 18
#define SIGKILL 9
#define SIGNAL_CNT 6

/* The rest of the POSIX set. Tenok delivers none of them, the numbers are
 * named so that a program written for a POSIX system compiles
 */
#define SIGHUP 1
#define SIGINT 2
#define SIGQUIT 3
#define SIGILL 4
#define SIGTRAP 5
#define SIGABRT 6
#define SIGBUS 7
#define SIGFPE 8
#define SIGSEGV 11
#define SIGPIPE 13
#define SIGALRM 14
#define SIGTERM 15
#define SIGCHLD 17
#define SIGTSTP 20
#define SIGTTIN 21
#define SIGTTOU 22
#define SIGURG 23
#define SIGXCPU 24
#define SIGXFSZ 25
#define SIGVTALRM 26
#define SIGPROF 27
#define SIGWINCH 28
#define SIGSYS 31

#define NSIG 32

#define SIG_DFL ((void (*)(int)) 0)
#define SIG_IGN ((void (*)(int)) 1)
#define SIG_ERR ((void (*)(int)) - 1)

#define SIG_BLOCK 0
#define SIG_UNBLOCK 1
#define SIG_SETMASK 2

#define SA_SIGINFO 0x2
#define SA_RESTART 0x10000000
#define SA_NOCLDSTOP 0x00000001

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
 * @brief  Wait for a signal and return siginfo
 * @param  set: Pointer to the signal set.
 * @param  info: For returning signal info (optional).
 * @retval int: The signal number on success and nonzero error number on error.
 */
int sigwaitinfo(const sigset_t *set, siginfo_t *info);

/**
 * @brief  Wait for a signal with timeout
 * @param  set: Pointer to the signal set.
 * @param  info: For returning signal info (optional).
 * @param  timeout: Relative timeout.
 * @retval int: The signal number on success and nonzero error number on error.
 */
int sigtimedwait(const sigset_t *set,
                 siginfo_t *info,
                 const struct timespec *timeout);

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
