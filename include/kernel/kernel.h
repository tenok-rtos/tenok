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

struct task_stack_fpu {
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

/* task control block */
struct task_ctrl_blk {
    struct task_stack *stack_top; //pointer that points to the stack top
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
    struct list task_list;
    struct list list;
};

void *kmalloc(size_t size);
void kfree(void *ptr);
int ktask_create(task_func_t task_func, uint8_t priority);

struct task_ctrl_blk *current_task_info(void);

void sched_start(void);

void os_env_init(uint32_t stack);
uint32_t *jump_to_user_space(uint32_t stack, bool privileged);

void prepare_to_wait(struct list *q, struct list *wait, int state);

void set_syscall_pending(struct task_ctrl_blk *task);
void reset_syscall_pending(struct task_ctrl_blk *task);

#endif
