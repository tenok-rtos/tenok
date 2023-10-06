/**
 * @file
 */
#ifndef __KERNEL_H__
#define __KERNEL_H__

#include <stdint.h>
#include <stdbool.h>
#include <signal.h>
#include <sys/resource.h>

#include <fs/fs.h>
#include <kernel/list.h>
#include <kernel/time.h>
#include <kernel/wait.h>

#include "kconfig.h"

#define TASK_CNT_MAX   32
#define THREAD_CNT_MAX 64

#define SIGNAL_CLEANUP_EVENT 100
#define THREAD_JOIN_EVENT    101

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

    /* file descriptors table */
    struct fdtable fdtable[FILE_DESC_CNT_MAX];
    int fd_cnt;

    struct list threads_list; /* all threads held by the task */
    struct list list;
};

struct thread_info {
    struct stack *stack_top;  /* stack pointer */
    uint32_t *stack;          /* base address to the thread stack */
    size_t stack_size;        /* bytes */

    struct task_struct *task; /* the task of this thread */

    struct {
        uint32_t *r0, *r1, *r2, *r3;
    } reg;

    uint8_t  status;
    uint32_t tid;
    int      priority;
    char     name[TASK_NAME_LEN_MAX];
    uint32_t privilege;
    bool     syscall_pending;
    uint32_t sleep_ticks; /* remained ticks to sleep before wake up */

    struct {
        size_t size;
    } file_request;

    /* signal table */
    int *ret_sig;
    bool wait_for_signal;
    sigset_t sig_wait_set; /* the signal set of the thread to wait */
    uint32_t stack_top_preserved;
    struct sigaction *sig_table[SIGNAL_CNT];

    /* timers */
    int timer_cnt;
    struct list timers_list;

    /* poll */
    bool poll_failed;
    wait_queue_head_t poll_wq;
    struct timespec poll_timeout;
    struct list poll_files_list;

    struct list task_list;   /* list head for the task to track */
    struct list thread_list; /* global list of threads */
    struct list poll_list;   /* list head for the poll handler to track */
    struct list join_list;   /* list head of other threads to wait for join of the thread */
    struct list list;        /* list head for thread scheduling */
};

int kthread_create(task_func_t task_func, uint8_t priority, int stack_size);

void set_syscall_pending(struct thread_info *task);
void reset_syscall_pending(struct thread_info *task);

struct task_struct *current_task_info(void);
struct thread_info *current_thread_info(void);

void sched_start(void);

#endif
