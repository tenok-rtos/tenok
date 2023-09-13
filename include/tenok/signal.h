#ifndef __SIGNAL_H__
#define __SIGNAL_H__

#include <tenok/sys/types.h>

/* supported signals by tenok */
#define SIGUSR1  10
#define SIGUSR2  12
#define SIGALRM  14
#define SIGPOLL  29
#define SIGSTOP  19
#define SIGCONT  18
#define SIGKILL  9

#define SIGNAL_CNT 7

#define SA_SIGINFO 0x2

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

int sigemptyset(sigset_t *set);
int sigfillset(sigset_t *set);
int sigaddset(sigset_t *set, int signum);
int sigdelset(sigset_t *set, int signum);
int sigaction(int signum, const struct sigaction *act,
              struct sigaction *oldact);
int kill(pid_t pid, int sig);

#endif
