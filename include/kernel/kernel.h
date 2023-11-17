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

#define SYSCALL_ARG(type, idx) *((type *) running_thread->args[idx])

#define CURRENT_TASK_INFO(var) struct task_struct *var = current_task_info()
#define CURRENT_THREAD_INFO(var) struct thread_info *var = current_thread_info()

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
    int pid;

    uint32_t bitmap_fds[BITMAP_SIZE(FILE_DESC_CNT_MAX)];
    uint32_t bitmap_mqds[BITMAP_SIZE(MQUEUE_CNT_MAX)];

    struct thread_info *main_thread;

    struct list_head threads_list; /* all threads held by the task */
    struct list_head list;
};

struct thread_info {
    unsigned long *stack_top; /* stack pointer */
    unsigned long *stack;     /* base address to the thread stack */
    size_t stack_size;        /* bytes */

    unsigned long *args[4]; /* pointer to the syscall arguments in the staack */

    struct task_struct *task; /* the task of this thread */

    uint8_t status;
    uint32_t tid;
    uint8_t priority;
    uint8_t original_priority; /* used by mutex priority inheritance */
    bool priority_inherited;   /* to mark that if current priority is
                                * inherited from another thread */
    char name[THREAD_NAME_MAX];
    uint32_t privilege;
    uint32_t sleep_ticks; /* remained ticks to sleep before wake up */

    /* thread control */
    bool detached;
    bool joinable;
    void *retval;
    struct thread_once *once_control;

    /* syscall */
    bool syscall_pending;
    bool syscall_is_timeout;
    struct timespec syscall_timeout;

    /* file */
    struct {
        size_t size;
    } file_request;

    /* signals */
    int *ret_sig;
    bool wait_for_signal;  /* indicates that the thread is waiting for signal */
    sigset_t sig_wait_set; /* the signal set to wait */
    uint32_t signal_cnt;   /* the number of pending signals in the queue */
    uint32_t stack_top_preserved; /* original stack address before an event
                                     handler is staged */
    struct kfifo signal_queue;    /* the queue for pending signals */
    struct sigaction *sig_table[SIGNAL_CNT];

    /* timers */
    int timer_cnt;
    struct list_head timers_list; /* list of timers of the thread */

    /* poll */
    struct list_head
        poll_files_list; /* list of all files the thread is waiting */

    struct list_head task_list;    /* link to the global task list */
    struct list_head thread_list;  /* link to the global thread list */
    struct list_head timeout_list; /* link to the global timeout list */
    struct list_head join_list;    /* link to another thread waiting for join */
    struct list_head list;         /* link to a scheduling list */
};

int kthread_create(task_func_t task_func, uint8_t priority, int stack_size);

struct task_struct *current_task_info(void);
struct thread_info *current_thread_info(void);
struct thread_info *acquire_thread(int tid);

#endif
