/**
 * @file
 */
#ifndef __KERNEL_H__
#define __KERNEL_H__

#include <stdint.h>
#include <stdbool.h>
#include <signal.h>
#include <pthread.h>
#include <sys/resource.h>

#include <fs/fs.h>
#include <kernel/list.h>
#include <kernel/time.h>
#include <kernel/wait.h>
#include <kernel/util.h>

#include "kconfig.h"

#define TASK_CNT_MAX   32
#define THREAD_CNT_MAX 64

#define DEF_SYSCALL(func, _num) \
        {.syscall_handler = sys_ ## func, .num = _num}

#define SYSCALL_ARG(type, arg_num) \
    *(type *)running_thread->reg.r ## arg_num

#define CURRENT_TASK_INFO(var) struct task_struct *var = current_task_info()
#define CURRENT_THREAD_INFO(var) struct thread_info *var = current_thread_info()

#define SUPERVISOR_EVENT(thread) thread->stack_top->_r7

typedef void (*task_func_t)(void);
typedef void (*thread_func_t)(void);

typedef struct {
    void (* syscall_handler)(void);
    uint32_t num;
} syscall_info_t;

enum {
    THREAD_WAIT,
    THREAD_READY,
    THREAD_RUNNING,
    THREAD_SUSPENDED,
    THREAD_TERMINATED
} THREAD_STATUS;

enum {
    KERNEL_THREAD = 0,
    USER_THREAD = 1
} THREAD_TYPE;

/* stack layout for threads without using fpu */
struct stack {
    /* registers pushed into the stack by the os */
    uint32_t r4_to_r11[8]; /* r4, ..., r11 */
    uint32_t _lr;
    uint32_t _r7;          /* r7 (syscall number) */

    /* registers pushed into the stack by exception entry */
    uint32_t r0, r1, r2, r3;
    uint32_t r12_lr_pc_xpsr[4]; /* r12, lr, pc, xpsr */

    /* the rest of the stack is ommited */
};

/* stack layout for threads that using fpu */
struct stack_fpu {
    /* registeres pushed into the stack by the os */
    uint32_t r4_to_r11[8];   /* r4, ..., r11 */
    uint32_t _lr;
    uint32_t _r7;            /* r7 (syscall number) */
    uint32_t s16_to_s31[16]; /* s16, ..., s31 */

    /* registers pushed into the stack by exception entry */
    uint32_t r0, r1, r2, r3;
    uint32_t r12_lr_pc_xpsr[4];   /* r12, lr, pc, xpsr */
    uint32_t s0_to_s15_fpscr[17]; /* s0, ..., s15, fpscr */

    /* the rest of the stack is ommited */
};

struct task_struct {
    int pid;

    uint32_t bitmap_fds[BITMAP_SIZE(FILE_DESC_CNT_MAX)];
    uint32_t bitmap_mqds[BITMAP_SIZE(MQUEUE_CNT_MAX)];

    struct thread_info *main_thread;

    struct list_head threads_list; /* all threads held by the task */
    struct list_head list;
};

struct thread_info {
    struct stack *stack_top;  /* stack pointer */
    uint32_t *stack;          /* base address to the thread stack */
    size_t stack_size;        /* bytes */

    struct {
        uint32_t *r0, *r1, *r2, *r3;
    } reg;

    struct task_struct *task; /* the task of this thread */

    uint8_t  status;
    uint32_t tid;
    uint8_t  priority;
    char     name[TASK_NAME_LEN_MAX];
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
    sigset_t sig_wait_set; /* the signal set of the thread to wait */
    uint32_t stack_top_preserved; /* original stack address before any event handler is staged */
    struct sigaction *sig_table[SIGNAL_CNT];

    /* timers */
    int timer_cnt;
    struct list_head timers_list; /* list of timers of the thread */

    /* poll */
    struct list_head poll_files_list; /* list of all files the thread is waiting */

    struct list_head task_list;    /* connect to the global task list */
    struct list_head thread_list;  /* connect to the global thread list */
    struct list_head timeout_list; /* connect to the global timeout list */
    struct list_head join_list;    /* connect to another thread waiting for join */
    struct list_head list;         /* connect to a scheduling list (ready list, wait list, etc.) */
};

int kthread_create(task_func_t task_func, uint8_t priority, int stack_size);

struct task_struct *current_task_info(void);
struct thread_info *current_thread_info(void);
struct thread_info *acquire_thread(int tid);

#endif
