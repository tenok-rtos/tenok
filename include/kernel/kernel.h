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

#define DEF_SYSCALL(func, _num) \
        {.syscall_handler = sys_ ## func, .num = _num}

#define SYSCALL_ARG(type, arg_num) \
    *(type *)running_thread->reg.r ## arg_num

#define CURRENT_TASK_INFO(var) struct task_struct *var = current_task_info()
#define CURRENT_THREAD_INFO(var) struct thread_info *var = current_thread_info()

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

/* stack layout for threads without using fpu */
struct stack {
    /* registers pushed into the stack by the os */
    uint32_t r4_to_r11[8]; /* r4, ..., r11 */
    uint32_t _lr;
    uint32_t _r7;          /* r7 (syscall number) */

    /* registers pushed into the stack by exception entry */
    uint32_t r0, r1, r2, r3;
    uint32_t r12_lr_pc_xpsr[4]; /* r12, lr, pc, xpsr */

    /* rest of the stack */
    uint32_t stack[TASK_STACK_SIZE - 17];
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

    /* rest of the stack */
    uint32_t stack[TASK_STACK_SIZE - 50];
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
    struct stack *stack_top;         /* stack pointer */
    uint32_t stack[TASK_STACK_SIZE]; /* stack memory */
    uint32_t stack_size;

    struct task_struct *task;        /* the task of this thread */

    struct {
        uint32_t *r0, *r1, *r2, *r3;
    } reg;

    uint8_t  status;
    uint32_t tid;
    int      priority;
    char     name[TASK_NAME_LEN_MAX];
    bool     privileged;
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
    struct list list;        /* list head for thread scheduling */
};

void *kmalloc(size_t size);
void kfree(void *ptr);
int ktask_create(task_func_t task_func, uint8_t priority, int stack_size);

void set_syscall_pending(struct thread_info *task);
void reset_syscall_pending(struct thread_info *task);

struct task_struct *current_task_info(void);
struct thread_info *current_thread_info(void);

void sched_start(void);

#endif
