#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "stm32f4xx.h"
#include "kernel.h"
#include "syscall.h"
#include "kconfig.h"
#include "list.h"
#include "mpool.h"
#include "fs.h"
#include "fifo.h"
#include "mqueue.h"

#define HANDLER_MSP  0xFFFFFFF1
#define THREAD_MSP   0xFFFFFFF9
#define THREAD_PSP   0xFFFFFFFD

#define INITIAL_XPSR 0x01000000

void sys_sched_yield(void);
void sys_set_irq(void);
void sys_set_program_name(void);
void sys_fork(void);
void sys_sleep(void);
void sys_mount(void);
void sys_open(void);
void sys_read(void);
void sys_write(void);
void sys_lseek(void);
void sys_fstat(void);
void sys_opendir(void);
void sys_readdir(void);
void sys_getpriority(void);
void sys_setpriority(void);
void sys_getpid(void);
void sys_mknod(void);

/* task lists */
struct list ready_list[TASK_MAX_PRIORITY + 1];
struct list sleep_list;

/* tasks */
struct task_ctrl_blk tasks[TASK_CNT_MAX];
struct task_ctrl_blk *running_task = NULL;
int task_cnt = 0;

/* memory pool */
struct memory_pool mem_pool;
uint8_t mem_pool_buf[MEM_POOL_SIZE];

/* file table */
struct file *files[TASK_CNT_MAX + FILE_CNT_MAX];
int file_cnt = 0;

/* syscall table */
syscall_info_t syscall_table[] = {
    /* non-posix syscalls */
    DEF_SYSCALL(sched_yield, 1),
    DEF_SYSCALL(set_irq, 2),
    DEF_SYSCALL(set_program_name, 3),
    /* posix syscalls */
    DEF_SYSCALL(fork, 4),
    DEF_SYSCALL(sleep, 5),
    DEF_SYSCALL(mount, 6),
    DEF_SYSCALL(open, 7),
    DEF_SYSCALL(read, 8),
    DEF_SYSCALL(write, 9),
    DEF_SYSCALL(lseek, 10),
    DEF_SYSCALL(fstat, 11),
    DEF_SYSCALL(opendir, 12),
    DEF_SYSCALL(readdir, 13),
    DEF_SYSCALL(getpriority, 14),
    DEF_SYSCALL(setpriority, 15),
    DEF_SYSCALL(getpid, 16),
    DEF_SYSCALL(mknod, 17),
};

int syscall_table_size = sizeof(syscall_table) / sizeof(syscall_info_t);

void task_create(task_func_t task_func, uint8_t priority)
{
    if(task_cnt > TASK_CNT_MAX) {
        return;
    }

    tasks[task_cnt].pid = task_cnt;
    tasks[task_cnt].status = TASK_WAIT;
    tasks[task_cnt].priority = priority;
    tasks[task_cnt].stack_size = TASK_STACK_SIZE; //TODO: variable size?

    /*
     * stack design contains three parts:
     * xpsr, pc, lr, r12, r3, r2, r1, r0, (for setup exception return),
     * _r7 (for passing system call number), and
     * _lr, r11, r10, r9, r8, r7, r6, r5, r4 (for context switch)
     */
    uint32_t *stack_top = tasks[task_cnt].stack + tasks[task_cnt].stack_size - 18;
    stack_top[17] = INITIAL_XPSR;
    stack_top[16] = (uint32_t)task_func; // lr = task_entry
    stack_top[8]  = THREAD_PSP;          //_lr = 0xfffffffd
    tasks[task_cnt].stack_top = (struct task_stack *)stack_top;

    task_cnt++;
}

/* put the task pointed by the "wait" into the "wait_list", and change the task state. */
void prepare_to_wait(struct list *wait_list, struct list *wait, int state)
{
    list_push(wait_list, wait);

    struct task_ctrl_blk *task = list_entry(wait, struct task_ctrl_blk, list);
    task->status = state;
}

/*
 * move the task from the wait_list into the ready_list.
 * the task will not be executed immediately and requires
 * to wait until the scheduler select it.
 */
void wake_up(struct list *wait_list)
{
    /* pop the task from the waiting list */
    struct list *waken_task_list = list_pop(wait_list);
    struct task_ctrl_blk *waken_task = list_entry(waken_task_list, struct task_ctrl_blk, list);

    /* put the task into the ready list */
    list_push(&ready_list[waken_task->priority], &waken_task->list);
    waken_task->status = TASK_READY;
}

void schedule(void)
{
    /* check the sleep list */
    struct list *list_itr = sleep_list.next;
    while(list_itr != &sleep_list) {
        struct list *next = list_itr->next;
        struct task_ctrl_blk *task = list_entry(list_itr, struct task_ctrl_blk, list);

        /* task is ready, push it into the ready list according to its priority */
        if(task->remained_ticks == 0) {
            task->status = TASK_READY;
            list_remove(list_itr); //remove the task from the sleep list
            list_push(&ready_list[task->priority], list_itr);
        }

        list_itr = next;
    }

    /* find a ready list that contains runnable tasks */
    int pri;
    for(pri = TASK_MAX_PRIORITY; pri >= 0; pri--) {
        if(list_is_empty(&ready_list[pri]) == false) {
            break;
        }
    }

    /* task returned to the scheduler before the time is up */
    if(running_task->status == TASK_RUNNING) {
        /* check if any higher priority task is woken */
        if(pri > running_task->priority) {
            /* yes, suspend the current task */
            prepare_to_wait(&sleep_list, &running_task->list, TASK_WAIT);
        } else {
            /* no, keep running the current task */
            return;
        }
    }

    /* select a task from the ready list */
    struct list *next = list_pop(&ready_list[pri]);
    running_task = list_entry(next, struct task_ctrl_blk, list);
    running_task->status = TASK_RUNNING;
}

void systick_handler(void)
{
    struct list *curr;

    /* freeze the current task if it is still running */
    if(running_task->status == TASK_RUNNING) {
        prepare_to_wait(&sleep_list, &running_task->list, TASK_WAIT);
    }

    /* update the sleep timers */
    list_for_each(curr, &sleep_list) {
        /* get the task control block */
        struct task_ctrl_blk *task = list_entry(curr, struct task_ctrl_blk, list);

        /* update remained ticks of waiting */
        if(task->remained_ticks > 0) {
            task->remained_ticks--;
        }

    }
}

void syscall_handler(void)
{
    /* system call handler */
    int i;
    for(i = 0; i < syscall_table_size; i++) {
        if(running_task->stack_top->r7 == syscall_table[i].num) {
            /* execute the syscall service */
            syscall_table[i].syscall_handler();
            break;
        }
    }
}

void sys_sched_yield(void)
{
    /* suspend the current task */
    prepare_to_wait(&sleep_list, &running_task->list, TASK_WAIT);
}

void sys_set_irq(void)
{
    uint32_t state = *running_task->reg.r0;
    if(state) {
        //asm volatile ("cpsie i"); //enable all irq
        reset_basepri();
    } else {
        //asm volatile ("cpsid i"); //disable all irq
        set_basepri();
    }
}

void sys_set_program_name(void)
{
    /* read syscall argument */
    char *name = (char*)*running_task->reg.r0;

    strncpy(running_task->name, name, TASK_NAME_LEN_MAX);
}

void sys_fork(void)
{
    if(task_cnt > TASK_CNT_MAX) {
        *running_task->reg.r0 = -1; //pass the return value with r0
        return;
    }

    /* calculate the used space of the parent task's stack */
    uint32_t *parent_stack_end = running_task->stack + running_task->stack_size;
    uint32_t stack_used = parent_stack_end - (uint32_t *)running_task->stack_top;

    tasks[task_cnt].pid = task_cnt;
    tasks[task_cnt].priority = running_task->priority;
    tasks[task_cnt].stack_size = running_task->stack_size;
    tasks[task_cnt].stack_top = (struct task_stack *)(tasks[task_cnt].stack + tasks[task_cnt].stack_size - stack_used);

    /* put the forked task into the sleep list */
    tasks[task_cnt].status = TASK_WAIT;
    list_push(&sleep_list, &tasks[task_cnt].list);

    /* copy the stack of the used part only */
    memcpy(tasks[task_cnt].stack_top, running_task->stack_top, sizeof(uint32_t)*stack_used);

    /* return the child pid to the parent task */
    *running_task->reg.r0 = task_cnt;

    /*
     * select the proper stack layout and return the pid number to the child task
     * check lr[4] bits (0: fpu is used, 1: fpu is unused)
     */
    if(running_task->stack_top->_lr & 0x10) {
        struct task_stack *sp = (struct task_stack *)tasks[task_cnt].stack_top;
        sp->r0 = running_task->pid;
    } else {
        struct task_stack_fpu *sp_fpu = (struct task_stack_fpu *)tasks[task_cnt].stack_top;
        sp_fpu->r0 = running_task->pid;

    }

    task_cnt++;
}

void sys_sleep(void)
{
    /* reconfigure the timer for sleeping */
    running_task->remained_ticks = *running_task->reg.r0;

    /* put the task into the sleep list and change the status */
    running_task->status = TASK_WAIT;
    list_push(&sleep_list, &(running_task->list));

    /* pass the return value with r0 */
    *running_task->reg.r0 = 0;
}

void sys_mount(void)
{
    /* read syscall arguments */
    char *source = (char *)*running_task->reg.r0;
    char *target = (char *)*running_task->reg.r1;

    int task_fd = running_task->pid;

    /* if the pending flag is set that means the request is already sent */
    if(running_task->syscall_pending == false) {
        request_mount(task_fd, source, target);
    }

    /* receive the mount result */
    int result;
    if(fifo_read(files[task_fd], (char *)&result, sizeof(result), 0)) {
        //return the file descriptor number with r0
        *running_task->reg.r0 = result;
    }
}

void sys_open(void)
{
    /* read syscall argument */
    char *pathname = (char *)*running_task->reg.r0;

    int task_fd = running_task->pid;

    /* if the pending flag is set that means the request is already sent */
    if(running_task->syscall_pending == false) {
        request_open_file(task_fd, pathname);
    }

    /* inquire the file descriptor from the file system task */
    int fd;
    if(fifo_read(files[task_fd], (char *)&fd, sizeof(fd), 0)) {
        //pass the file descriptor number with r0
        *running_task->reg.r0 = fd;
    }
}

void sys_read(void)
{
    /* read syscall argument */
    int fd = *running_task->reg.r0;
    char *buf = (uint8_t *)*running_task->reg.r1;
    size_t count = *running_task->reg.r2;

    /* get the file pointer from the table */
    struct file *filp = files[fd];

    /* read the file */
    ssize_t retval = filp->f_op->read(filp, buf, count, 0);

    /* pass the return value only if the read operation is complete */
    if(running_task->syscall_pending == false) {
        *running_task->reg.r0 = retval;
    }
}

void sys_write(void)
{
    /* read syscall arguments */
    int fd = *running_task->reg.r0;
    char *buf = (uint8_t *)*running_task->reg.r1;
    size_t count = *running_task->reg.r2;

    /* get the file pointer from the table */
    struct file *filp = files[fd];

    /* write the file */
    ssize_t retval = filp->f_op->write(filp, buf, count, 0);

    *running_task->reg.r0 = retval; //pass the return value with r0
}

void sys_lseek(void)
{
    /* read syscall arguments */
    int fd = *running_task->reg.r0;
    long offset = *running_task->reg.r1;
    int whence = *running_task->reg.r2;

    /* get the file pointer from the table */
    struct file *filp = files[fd];

    /* adjust call lseek implementation */
    int retval = filp->f_op->llseek(filp, offset, whence);

    *running_task->reg.r0 = retval; //pass the return value with r0
}

void sys_fstat(void)
{
    /* read syscall arguments */
    int fd = *running_task->reg.r0;
    struct stat *statbuf = (struct stat *)*running_task->reg.r1;

    /* read the inode of the given file */
    struct inode *inode = files[fd]->file_inode;

    /* check if the inode exists */
    if(inode != NULL) {
        statbuf->st_mode   = inode->i_mode;
        statbuf->st_ino    = inode->i_ino;
        statbuf->st_rdev   = inode->i_rdev;
        statbuf->st_size   = inode->i_size;
        statbuf->st_blocks = inode->i_blocks;
    }

    *running_task->reg.r0 = 0; //pass the return value with r0
}

void sys_opendir(void)
{
    /* read syscall arguments */
    char *pathname = (char *)*running_task->reg.r0;
    DIR *dirp = (DIR *)*running_task->reg.r1;

    int task_fd = running_task->pid;

    /* if the pending flag is set that means the request is already sent */
    if(running_task->syscall_pending == false) {
        request_open_directory(task_fd, pathname);
    }

    /* inquire the file descriptor from the file system task */
    struct inode *inode_dir;
    if(fifo_read(files[task_fd], (char *)&inode_dir, sizeof(inode_dir), 0)) {
        //return the directory
        dirp->inode_dir = inode_dir;
        dirp->dentry_list = inode_dir->i_dentry.next;

        /* pass the return value with r0 */
        if(dirp->inode_dir == NULL) {
            *running_task->reg.r0 = -1;
        } else {
            *running_task->reg.r0 = 0;
        }
    }
}

void sys_readdir(void)
{
    /* read syscall arguments */
    DIR *dirp = (DIR *)*running_task->reg.r0;
    struct dirent *dirent = (struct dirent *)*running_task->reg.r1;

    /* pass the return value with r0 */
    *running_task->reg.r0 = (uint32_t)fs_read_dir(dirp, dirent);
}

void sys_getpriority(void)
{
    /* return the task priority with r0 */
    *running_task->reg.r0 = running_task->priority;
}

void sys_setpriority(void)
{
    /* read syscall arguments */
    int which = *running_task->reg.r0;
    int who = *running_task->reg.r1;
    int priority = *running_task->reg.r2;

    /* unsupported type of the `which' argument */
    if(which != PRIO_PROCESS) {
        running_task->stack_top->r0 = -1; //return -1 with r0
        return;
    }

    /* compare pid and set new priority */
    int i;
    for(i = 0; i < task_cnt; i++) {
        if(tasks[i].pid == who) {
            tasks[i].priority = priority;
            *running_task->reg.r0 = 0;  //return 0 with r0
            return;
        }
    }

    /* process not found */
    running_task->stack_top->r0 = -1; //return -1 with r0
}

void sys_getpid(void)
{
    /* return the task pid with r0 */
    *running_task->reg.r0 = running_task->pid;
}

void sys_mknod(void)
{
    /* read syscall argument */
    char *pathname = (char *)*running_task->reg.r0;
    //mode_t mode = (mode_t)*running_task->reg.r1;
    dev_t dev = (dev_t)*running_task->reg.r2;

    int task_fd = running_task->pid;

    /* if the pending flag is set that means the request is already sent */
    if(running_task->syscall_pending == false) {
        request_create_file(task_fd, pathname, dev);
    }

    /* wait until the file system complete the request */
    int new_fd;
    if(fifo_read(files[task_fd], (char *)&new_fd, sizeof(new_fd), 0) == 0) {
        return; //not ready
    }

    /* pass the return value with r0 */
    if(new_fd == -1) {
        *running_task->reg.r0 = -1;
    } else {
        *running_task->reg.r0 = 0;
    }
}

uint32_t get_proc_mode(void)
{
    /*
     * get the 9 bits isr number from the ipsr register,
     * check "exception types" of the ARM CM3 for details
     */
    volatile unsigned int mode;
    asm volatile ("mrs  r0, ipsr \n"
                  "str  r0, [%0] \n"
                  :: "r"(&mode));
    return mode & 0x1ff; //return ipsr[8:0]
}

void reset_basepri(void)
{
    asm volatile ("mov r0,      #0\n"
                  "msr basepri, r0\n");
}

void set_basepri(void)
{
    asm volatile ("mov r0,      %0\n"
                  "msr basepri, r0\n"
                  :: "i"(SYSCALL_INTR_PRI << 4));
}

void preempt_disable(void)
{
    if(get_proc_mode() == 0) {
        /* unprivileged thread mode (assumed to be called by the user task) */
        set_irq(0);
    } else {
        /* handler mode (i.e., interrupt, which is always privileged) */
        //asm volatile ("cpsid i");
        set_basepri();
    }
}

void preempt_enable(void)
{
    if(get_proc_mode() == 0) {
        /* unprivileged thread mode (assumed to be called by the user task) */
        set_irq(1);
    } else {
        /* handler mode (i.e., interrupt, which is always privileged) */
        //asm volatile ("cpsie i");
        reset_basepri();
    }
}

void os_service_init(void)
{
    rootfs_init();

    if(!fork()) file_system_task();
}

void sched_start(task_func_t first_task)
{
    /* set interrupt priorities */
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_4);
    NVIC_SetPriority(SysTick_IRQn, SYSCALL_INTR_PRI);
    NVIC_SetPriority(SVCall_IRQn, 0);

    uint32_t stack_empty[32]; //a dummy stack for os enviromnent initialization
    os_env_init((uint32_t)(stack_empty + 32) /* point to the top */);

    /* initialize the memory pool */
    memory_pool_init(&mem_pool, mem_pool_buf, MEM_POOL_SIZE);

    /* initialize fifos for all tasks */
    int i;
    for(i = 0; i < TASK_CNT_MAX; i++) {
        fifo_init(i, (struct file **)&files, NULL, &mem_pool);
    }

    /* initialize the task ready lists */
    for(i = 0; i <= TASK_MAX_PRIORITY; i++) {
        list_init(&ready_list[i]);
    }

    /* initialize the sleep list */
    list_init(&sleep_list);

    task_create(first_task, 0);

    /* enable the systick timer */
    SysTick_Config(SystemCoreClock / OS_TICK_FREQ);

    /* configure the first task to run */
    running_task = &tasks[0];
    tasks[0].status = TASK_RUNNING;

    while(1) {
        /* execute the task if the pending flag of the syscall is not set */
        if(running_task->syscall_pending == false) {
            running_task->stack_top =
                (struct task_stack *)jump_to_user_space((uint32_t)running_task->stack_top);

            /* record the stack address of r0-r3 due to the distinct stack layouts by lazy context switch mechanism of the fpu */
            if(running_task->stack_top->_lr & 0x10) { /* check lr[4] bits (0: fpu is used, 1: fpu is unused) */
                struct task_stack *sp = (struct task_stack *)running_task->stack_top;
                running_task->reg.r0 = &sp->r0;
                running_task->reg.r1 = &sp->r1;
                running_task->reg.r2 = &sp->r2;
                running_task->reg.r3 = &sp->r3;
            } else {
                struct task_stack_fpu *sp_fpu = (struct task_stack_fpu *)running_task->stack_top;
                running_task->reg.r0 = &sp_fpu->r0;
                running_task->reg.r1 = &sp_fpu->r1;
                running_task->reg.r2 = &sp_fpu->r2;
                running_task->reg.r3 = &sp_fpu->r3;
            }
        }

        /* _r7 is negative if the kernel is returned from the systick irq */
        if((int)running_task->stack_top->_r7 < 0) {
            running_task->stack_top->_r7 *= -1; //restore _r7
            systick_handler();
        } else {
            /* _r7 is positive if the kernel is returned from the syscall */
            syscall_handler();
        }

        schedule();
    }
}

void sprint_tasks(char *str, size_t size)
{
    int pos = snprintf(&str[0], size, "PID\tPR\tNAME\n\r");

    int i;
    for(i = 0; i < task_cnt; i++) {
        pos += snprintf(&str[pos], size, "%d\t%d\t%s\n\r", tasks[i].pid, tasks[i].priority, tasks[i].name);
    }
}
