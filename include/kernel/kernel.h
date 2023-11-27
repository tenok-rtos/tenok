/**
 * @file
 */
#ifndef __KERNEL_H__
#define __KERNEL_H__

#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <sys/limits.h>
#include <sys/resource.h>
#include <task.h>

#include <common/list.h>
#include <common/util.h>
#include <fs/fs.h>
#include <kernel/kfifo.h>
#include <kernel/time.h>
#include <kernel/wait.h>

#include "kconfig.h"

#define DEF_SYSCALL(func, _num)                                 \
    {                                                           \
        .handler_func = (unsigned long) sys_##func, .num = _num \
    }

#define SYSCALL_ARG(thread, type, idx) *((type *) thread->syscall_args[idx])

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
    unsigned long handler_func;
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
    uint16_t pid;                    /* Task ID */
    struct thread_info *main_thread; /* The main thread belongs to the task */

    /* For recording for the file descriptors belongs to the task */
    uint32_t bitmap_fds[BITMAP_SIZE(OPEN_MAX)];

    /* For recording message queue descriptors belongs to the task */
    uint32_t bitmap_mqds[BITMAP_SIZE(MQUEUE_MAX)];

    struct list_head threads_list; /* List of all threads of the task */
    struct list_head list;         /* Link to the global task list */
};

struct thread_info {
    /* Task */
    struct task_struct *task; /* Task of the thread belongs to */

    /* Stack */
    unsigned long *stack_top; /* Stack pointer to the top of the thread stack */
    unsigned long stack_top_preserved; /* Preserve for staging new handler */
    unsigned long *stack;              /* Base address of the thread stack */
    size_t stack_size;                 /* Stack size of the thread in bytes */

    /* Syscall */
    unsigned long *syscall_args[4];  /* Pointer to the syscall arguments */
    struct timespec syscall_timeout; /* For setting timeout for syscall */
    bool syscall_pending;    /* Indicate if the syscall is pending or not */
    bool syscall_is_timeout; /* Indicate if the syscall waiting time is up */

    /* Thread */
    void *retval;               /* For passing retval after the thread end */
    void **retval_join;         /* To receive retval from a thread to join */
    size_t file_request_size;   /* Size of the thread requesting to a file */
    uint32_t sleep_ticks;       /* Remained ticks to sleep */
    uint16_t tid;               /* Thread id */
    uint16_t timer_cnt;         /* The number of timers that the thread has */
    uint8_t privilege;          /* Kernel thread or user thread */
    uint8_t status;             /* Thread status */
    uint8_t priority;           /* Thread priority */
    uint8_t original_priority;  /* Original priority before inheritance */
    bool priority_inherited;    /* True if current priority is inherited */
    bool detached;              /* Thread is detached or not */
    bool joinable;              /* Thread is joinable or not */
    char name[THREAD_NAME_MAX]; /* Thread name */
    struct thread_once *once_control; /* For handling pthread_once_control */

    /* Signals */
    struct sigaction *sig_table[SIGNAL_CNT];
    struct kfifo signal_queue; /* The queue for pending signals */
    sigset_t sig_wait_set;     /* The signal set to wait */
    uint32_t signal_cnt;       /* The number of pending signals in the queue */
    int *ret_sig;              /* For storing retval of the sigwait */
    bool wait_for_signal;      /* Indicates the thread is waiting for signal */

    /* Lists */
    struct list_head timers_list;     /* List of timers belongs to the thread */
    struct list_head poll_files_list; /* List of all files polling for */
    struct list_head task_list;       /* Link to the thread list of the task */
    struct list_head thread_list;     /* Link to the global thread list */
    struct list_head timeout_list;    /* Link to the global timeout list */
    struct list_head join_list; /* Link to another thread waiting for join */
    struct list_head list;      /* Link to a scheduling list */
};

struct task_struct *current_task_info(void);
struct thread_info *current_thread_info(void);
struct thread_info *acquire_thread(int tid);

void set_daemon_id(int daemon);
uint16_t get_daemon_id(int daemon);

#endif
