/**
 * @file
 */
#ifndef __KERNEL_H__
#define __KERNEL_H__

#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <sys/resource.h>
#include <task.h>

#include <common/list.h>
#include <common/util.h>
#include <fs/fs.h>
#include <kernel/kfifo.h>
#include <kernel/time.h>
#include <kernel/wait.h>

#include "kconfig.h"

#define TASK_CNT_MAX 32
#define THREAD_CNT_MAX 64

#define DEF_SYSCALL(func, _num)                    \
    {                                              \
        .syscall_handler = sys_##func, .num = _num \
    }

#define SYSCALL_ARG(type, idx) *((type *) running_thread->syscall_args[idx])

#define CURRENT_TASK_INFO(var) struct task_struct *var = current_task_info()
#define CURRENT_THREAD_INFO(var) struct thread_info *var = current_thread_info()

/* clang-format off */
#define DAEMON_LIST \
    DECLARE_DAEMON(SOFTIRQD), \
    DECLARE_DAEMON(FILESYSD)
/* clang-format on */

#define DECLARE_DAEMON(x) x
enum {
    DAEMON_LIST,
    DAEMON_CNT,
};
#undef DECLARE_DAEMON

struct syscall_info {
    void (*syscall_handler)(void);
    uint32_t num;
};

struct staged_handler_info {
    uint32_t func;
    uint32_t args[4];
};

enum {
    THREAD_WAIT,
    THREAD_READY,
    THREAD_RUNNING,
    THREAD_SUSPENDED,
    THREAD_TERMINATED,
} THREAD_STATUS;

enum {
    KERNEL_THREAD = 0,
    USER_THREAD = 1,
} THREAD_TYPE;

struct task_struct {
    uint16_t pid;                    /* task id */
    struct thread_info *main_thread; /* the main thread belongs to the task */

    /* record for the file descriptors owned by the task */
    uint32_t bitmap_fds[BITMAP_SIZE(FILE_DESC_CNT_MAX)];

    /* record for the mqueue descriptors owned by the task */
    uint32_t bitmap_mqds[BITMAP_SIZE(MQUEUE_CNT_MAX)];

    struct list_head threads_list; /* list of all threads held by the task */
    struct list_head list;         /* link to the global task list */
};

struct thread_info {
    /* task */
    struct task_struct *task; /* task of the thread belongs to */

    /* stack */
    unsigned long *stack_top; /* stack pointer to the top of the thread stack */
    unsigned long stack_top_preserved; /* preserve for staging new handler */
    unsigned long *stack;              /* base address of the thread stack */
    size_t stack_size;                 /* stack size of the thread in bytes */

    /* syscall */
    unsigned long *syscall_args[4];  /* pointer to the syscall arguments */
    struct timespec syscall_timeout; /* for setting timeout for syscall */
    bool syscall_pending;    /* indicate if the syscall is pending or not */
    bool syscall_is_timeout; /* indicate if the syscall waiting time is up */

    /* thread */
    void *retval;               /* for passing retval after the thread end */
    void **retval_join;         /* to receive retval from a thread to join */
    size_t file_request_size;   /* size of the thread requesting to a file */
    uint32_t sleep_ticks;       /* remained ticks to sleep */
    uint16_t tid;               /* thread id */
    uint16_t timer_cnt;         /* the number of timers that the thread has */
    uint8_t privilege;          /* kernel thread or user thread */
    uint8_t status;             /* thread status */
    uint8_t priority;           /* thread priority */
    uint8_t original_priority;  /* original priority before inheritance */
    bool priority_inherited;    /* true if current priority is inherited */
    bool detached;              /* thread is detached or not */
    bool joinable;              /* thread is joinable or not */
    char name[THREAD_NAME_MAX]; /* thread name */
    struct thread_once *once_control; /* for handling pthread_once_control */

    /* signals */
    struct sigaction *sig_table[SIGNAL_CNT];
    struct kfifo signal_queue; /* the queue for pending signals */
    sigset_t sig_wait_set;     /* the signal set to wait */
    uint32_t signal_cnt;       /* the number of pending signals in the queue */
    int *ret_sig;              /* for storing retval of the sigwait */
    bool wait_for_signal;      /* indicates the thread is waiting for signal */

    /* lists */
    struct list_head timers_list;     /* list of timers belongs to the thread */
    struct list_head poll_files_list; /* list of all files polling for */
    struct list_head task_list;       /* link to the thread list of the task */
    struct list_head thread_list;     /* link to the global thread list */
    struct list_head timeout_list;    /* link to the global timeout list */
    struct list_head join_list; /* link to another thread waiting for join */
    struct list_head list;      /* link to a scheduling list */
};

int kthread_create(task_func_t task_func, uint8_t priority, int stack_size);

struct task_struct *current_task_info(void);
struct thread_info *current_thread_info(void);
struct thread_info *acquire_thread(int tid);

void set_daemon_id(int daemon);
uint16_t get_daemon_id(int daemon);

#endif
