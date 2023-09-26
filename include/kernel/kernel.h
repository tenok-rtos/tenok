#ifndef __KERNEL_H__
#define __KERNEL_H__

#include <stdint.h>
#include <stdbool.h>

#include <fs/fs.h>
#include <kernel/list.h>
#include <kernel/time.h>
#include <kernel/wait.h>

#include <tenok/signal.h>
#include <tenok/sys/resource.h>

#include "kconfig.h"

#define TASK_PRIORITY_MIN 1

#define DEF_SYSCALL(func, _num) \
        {.syscall_handler = sys_ ## func, .num = _num}

#define DEF_IRQ_ENTRY(func, _num) \
        {.syscall_handler = func, .num = _num}

#define CURRENT_TASK_INFO(var) struct task_ctrl_blk *var = current_task_info()
#define TASK_ENTRY(task_list) container_of(task_list, struct task_ctrl_blk, list);

#define SYSCALL_ARG(type, arg_num) \
    *(type *)running_task->reg.r ## arg_num

typedef struct {
    void (* syscall_handler)(void);
    uint32_t num;
} syscall_info_t;

typedef void (*task_func_t)(void);

enum {
    TASK_WAIT,
    TASK_READY,
    TASK_RUNNING,
    TASK_SUSPENDED,
    TASK_TERMINATED
} TASK_STATUS;

/* layout of the user stack when the fpu is not used */
struct task_stack {
    /* pushed by the os */
    uint32_t r4;
    uint32_t r5;
    uint32_t r6;
    uint32_t r7;
    uint32_t r8;
    uint32_t r9;
    uint32_t r10;
    uint32_t r11;
    uint32_t _lr;

    uint32_t _r7; //syscall number

    /* pushed by the exception entry */
    uint32_t r0;
    uint32_t r1;
    uint32_t r2;
    uint32_t r3;
    uint32_t r12;
    uint32_t lr;
    uint32_t pc;
    uint32_t xpsr;
    uint32_t stack[TASK_STACK_SIZE - 17];
};

/* layout of the user stack when fpu is used */
struct task_stack_fpu {
    /* pushed by the os */
    uint32_t r4;
    uint32_t r5;
    uint32_t r6;
    uint32_t r7;
    uint32_t r8;
    uint32_t r9;
    uint32_t r10;
    uint32_t r11;
    uint32_t _lr;

    uint32_t _r7; //syscall number

    uint32_t s16;
    uint32_t s17;
    uint32_t s18;
    uint32_t s19;
    uint32_t s20;
    uint32_t s21;
    uint32_t s22;
    uint32_t s23;
    uint32_t s24;
    uint32_t s25;
    uint32_t s26;
    uint32_t s27;
    uint32_t s28;
    uint32_t s29;
    uint32_t s30;
    uint32_t s31;

    /* pushed by the exception entry */
    uint32_t r0;
    uint32_t r1;
    uint32_t r2;
    uint32_t r3;
    uint32_t r12;
    uint32_t lr;
    uint32_t pc;
    uint32_t xpsr;
    //s0
    //...
    //s15
    //fpscr

    uint32_t stack[TASK_STACK_SIZE - 33];
};

/* task control block */
struct task_ctrl_blk {
    struct task_stack *stack_top;    //pointer of the stack top address
    uint32_t stack[TASK_STACK_SIZE]; //stack memory
    uint32_t stack_size;

    struct {
        uint32_t *r0, *r1, *r2, *r3;
    } reg;

    uint8_t  status;
    uint32_t pid;
    int      priority;
    char     name[TASK_NAME_LEN_MAX];
    bool     privileged;

    uint32_t sleep_ticks;

    bool     syscall_pending;

    struct {
        size_t size;
    } file_request;

    /* file descriptor table */
    struct fdtable fdtable[FILE_DESC_CNT_MAX];
    int fd_cnt;

    /* signal table */
    uint32_t stack_top_preserved;
    struct sigaction *sig_table[SIGNAL_CNT];
    sigset_t sig_wait_set;
    bool wait_for_signal;
    int *ret_sig;

    /* timers */
    struct list timer_head;
    int timer_cnt;

    /* poll */
    wait_queue_head_t poll_wq;
    struct list poll_files_head;
    struct list poll_list;
    struct timespec poll_timeout;
    bool poll_failed;

    /* task list */
    struct list list;
};

void *kmalloc(size_t size);
void kfree(void *ptr);
int ktask_create(task_func_t task_func, uint8_t priority);

struct task_ctrl_blk *current_task_info(void);

void os_service_init(void);
void sched_start(void);

void os_env_init(uint32_t stack);
uint32_t *jump_to_user_space(uint32_t stack, bool privileged);

void prepare_to_wait(struct list *q, struct list *wait, int state);

void preempt_disable(void);
void preempt_enable(void);

uint32_t get_proc_mode(void);

void reset_basepri(void);
void set_basepri(void);

void set_syscall_pending(void);
void reset_syscall_pending(void);

#endif
