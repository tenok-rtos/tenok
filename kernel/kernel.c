#include <tenok.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <task.h>
#include <time.h>
#include <poll.h>
#include <errno.h>
#include <mqueue.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>
#include <semaphore.h>
#include <sys/stat.h>
#include <sys/mount.h>

#include <fs/fs.h>
#include <rom_dev.h>
#include <mm/page.h>
#include <mm/mpool.h>
#include <arch/port.h>
#include <kernel/list.h>
#include <kernel/ipc.h>
#include <kernel/time.h>
#include <kernel/wait.h>
#include <kernel/task.h>
#include <kernel/util.h>
#include <kernel/errno.h>
#include <kernel/printk.h>
#include <kernel/bitops.h>
#include <kernel/kernel.h>
#include <kernel/signal.h>
#include <kernel/syscall.h>
#include <kernel/interrupt.h>
#include <kernel/softirq.h>

#include "kconfig.h"
#include "stm32f4xx.h"

#define HANDLER_MSP  0xFFFFFFF1
#define THREAD_MSP   0xFFFFFFF9
#define THREAD_PSP   0xFFFFFFFD

#define INITIAL_XPSR 0x01000000

/* global lists */
struct list tasks_list;        /* global list for recording all tasks in the system */
struct list threads_list;      /* global list for recording all threads in the system */
struct list sleep_list;        /* list of all threads in the sleeping state */
struct list suspend_list;      /* list of all thread currently suspended */
struct list timers_list;       /* list of all timers in the system */
struct list signal_wait_list;  /* list of all threads waiting for signals */
struct list poll_timeout_list; /* list for tracking threads that setup timeout for poll() */
struct list ready_list[TASK_MAX_PRIORITY + 1]; /* lists of all threads that ready to run */

/* tasks and threads */
struct task_struct tasks[TASK_CNT_MAX];
struct thread_info threads[THREAD_CNT_MAX];
struct thread_info *running_thread = NULL;
uint32_t bitmap_tasks[2];   /* for dispatching task id */
uint32_t bitmap_threads[2]; /* for dispatching thread id */

/* memory pool */
struct memory_pool mem_pool;
uint8_t mem_pool_buf[MEM_POOL_SIZE];

/* files */
struct file *files[TASK_CNT_MAX + FILE_CNT_MAX];
int file_cnt = 0;

/* message queues */
struct msg_queue mq_table[MQUEUE_CNT_MAX];
int mq_cnt = 0;

/* syscall table */
syscall_info_t syscall_table[] = {
    SYSCALL_TABLE_INIT
};

NACKED void thread_return_handler(void)
{
    SYSCALL(THREAD_JOIN_EVENT);
}

NACKED void sig_return_handler(void)
{
    SYSCALL(SIGNAL_CLEANUP_EVENT);
}

void *kmalloc(size_t size)
{
    return memory_pool_alloc(&mem_pool, size);
}

void kfree(void *ptr)
{
    return;
}

static inline void set_syscall_pending(struct thread_info *thread)
{
    thread->syscall_pending = true;
}

static inline void reset_syscall_pending(struct thread_info *thread)
{
    thread->syscall_pending = false;
}

struct task_struct *current_task_info(void)
{
    return running_thread->task;
}

struct thread_info *current_thread_info(void)
{
    return running_thread;
}

static struct task_struct *acquire_task(int pid)
{
    struct task_struct *task;

    struct list *curr;
    list_for_each(curr, &tasks_list) {
        task = list_entry(curr, struct task_struct, list);
        if(task->pid == pid) {
            return task;
        }
    }

    return NULL;
}

static struct thread_info *acquire_thread(int tid)
{
    struct thread_info *thread;

    struct list *curr;
    list_for_each(curr, &threads_list) {
        thread = list_entry(curr, struct thread_info, thread_list);
        if(thread->tid == tid) {
            return thread;
        }
    }

    return NULL;
}

static struct thread_info *thread_create(thread_func_t thread_func, uint8_t priority,
        int stack_size, uint32_t privilege)
{
    /* allocate a new thread id */
    int tid = find_first_zero_bit(bitmap_threads, THREAD_CNT_MAX);
    if(tid >= THREAD_CNT_MAX)
        return NULL;
    bitmap_set_bit(bitmap_threads, tid);

    /* stack size alignment */
    stack_size = align_up(stack_size, 4);

    /* allocate a new thread */
    struct thread_info *thread = &threads[tid];

    /* reset thread data */
    memset(thread, 0, sizeof(struct thread_info));

    /* allocate stack for the new thread */
    thread->stack = alloc_pages(size_to_page_order(stack_size));

    /* stack design contains three parts:
     * xpsr, pc, lr, r12, r3, r2, r1, r0, (for setup exception return),
     * _r7 (for passing system call number), and
     * _lr, r11, r10, r9, r8, r7, r6, r5, r4 (for context switch)
     */
    size_t stack_size_word = stack_size / 4;
    uint32_t *stack_top = thread->stack + stack_size_word - 18;
    stack_top[17] = INITIAL_XPSR;
    stack_top[16] = (uint32_t)thread_func;           // pc = task_entry
    stack_top[15] = (uint32_t)thread_return_handler; // lr
    stack_top[8]  = THREAD_PSP;                      //_lr = 0xfffffffd

    thread->stack_top = (struct stack *)stack_top;
    thread->stack_size = stack_size;
    thread->status = THREAD_WAIT;
    thread->tid = tid;
    thread->priority = priority;
    thread->privilege = privilege;

    /* initialize the list for poll syscall */
    list_init(&thread->poll_files_list);

    /* initialize the thread join list */
    list_init(&thread->join_list);

    /* record the new thread in the global list */
    list_push(&threads_list, &thread->thread_list);

    /* put the new thread in the sleep list */
    list_push(&sleep_list, &thread->list);

    return thread;
}

static int _task_create(thread_func_t task_func, uint8_t priority,
                        int stack_size, uint32_t privilege)
{
    /* create a thread for the new task */
    struct thread_info *thread =
        thread_create(task_func, priority, stack_size, privilege);

    if(!thread)
        return -1;

    /* allocate a new task id */
    int pid = find_first_zero_bit(bitmap_tasks, TASK_CNT_MAX);
    if(pid >= TASK_CNT_MAX)
        return -1;
    bitmap_set_bit(bitmap_tasks, pid);

    /* allocate a new task */
    struct task_struct *task = &tasks[pid];
    task->pid = pid;
    list_init(&task->threads_list);
    list_push(&task->threads_list, &thread->task_list);
    list_push(&tasks_list, &task->list);

    /* reset file descriptor table */
    task->fd_cnt = 0;
    memset(task->fdtable, 0, sizeof(task->fdtable));

    /* set the task ownership of the thread */
    thread->task = task;

    return task->pid;
}

int kthread_create(task_func_t task_func, uint8_t priority, int stack_size)
{
    return _task_create(task_func, priority, stack_size, KERNEL_THREAD);
}

static void stage_temporary_handler(struct thread_info *thread,
                                    uint32_t func,
                                    uint32_t return_handler,
                                    uint32_t args[4])
{
    /* preserve the original stack pointer of the thread */
    thread->stack_top_preserved = (uint32_t)thread->stack_top;

    /* prepare a new space on user task's stack for the signal handler */
    uint32_t *stack_top = (uint32_t *)thread->stack_top - 18;
    stack_top[17] = INITIAL_XPSR;   //psr
    stack_top[16] = func;           //pc
    stack_top[15] = return_handler; //lr
    stack_top[14] = 0;              //r12 (ip)
    stack_top[13] = args[3];        //r3
    stack_top[12] = args[2];        //r2
    stack_top[11] = args[1];        //r1
    stack_top[10] = args[0];        //r0
    stack_top[8]  = THREAD_PSP;
    thread->stack_top = (struct stack *)stack_top;
}

static void stage_sigaction_handler(struct thread_info *thread,
                                    sa_sigaction_t sa_sigaction,
                                    int sigval,
                                    siginfo_t *info,
                                    void *context)
{
    uint32_t args[4];
    args[0] = (uint32_t)sigval;
    args[1] = (uint32_t)info;
    args[2] = (uint32_t)context;
    args[3] = 0;
    stage_temporary_handler(thread, (uint32_t)sa_sigaction, (uint32_t)sig_return_handler, args);
}

static void stage_signal_handler(struct thread_info *thread,
                                 sa_handler_t sa_handler,
                                 int sigval)
{
    uint32_t args[4] = {0};
    args[0] = (uint32_t)sigval;
    stage_temporary_handler(thread, (uint32_t)sa_handler, (uint32_t)sig_return_handler, args);
}

static void thread_suspend(struct thread_info *thread)
{
    if (thread->status == THREAD_SUSPENDED) {
        return;
    }

    prepare_to_wait(&suspend_list, &thread->list, THREAD_SUSPENDED);
}

static void thread_resume(struct thread_info *thread)
{
    if(thread->status != THREAD_SUSPENDED) {
        return;
    }

    thread->status = THREAD_READY;
    list_move(&thread->list, &ready_list[thread->priority]);
}

static void thread_delete(struct thread_info *thread)
{
    /* remove the thread from the system */
    list_remove(&thread->task_list);
    list_remove(&thread->thread_list);
    list_remove(&thread->list);
    thread->status = THREAD_TERMINATED;
    bitmap_clear_bit(bitmap_threads, thread->tid);

    /* free the stack memory */
    free_pages((uint32_t)thread->stack, size_to_page_order(thread->stack_size));

    /* remove the task from the system if it contains no thread anymore */
    struct task_struct *task = thread->task;
    if(list_is_empty(&task->threads_list)) {
        list_remove(&task->list);
        bitmap_clear_bit(bitmap_tasks, task->pid);
    }
}

void prepare_to_wait(struct list *wait_list, struct list *wait, int state)
{
    list_push(wait_list, wait);

    struct thread_info *thread = list_entry(wait, struct thread_info, list);
    thread->status = state;
}

void wake_up(struct list *wait_list)
{
    struct thread_info *highest_pri_thread = NULL;
    struct thread_info *thread;

    /* find the highest priority task in the waiting list */
    struct list *curr;
    list_for_each(curr, wait_list) {
        thread = list_entry(curr, struct thread_info, list);
        if(thread->priority > highest_pri_thread->priority ||
           highest_pri_thread == NULL) {
            highest_pri_thread = thread;
        }
    }

    /* no thread is found */
    if(!highest_pri_thread) {
        return;
    }

    /* wake up the thread by placing it back to the ready list */
    list_move(&highest_pri_thread->list, &ready_list[highest_pri_thread->priority]);
    highest_pri_thread->status = THREAD_READY;
}

void finish_wait(struct list *thread_list)
{
    struct thread_info *thread = list_entry(thread_list, struct thread_info, list);
    thread->status = THREAD_READY;
    list_move(thread_list, &ready_list[thread->priority]);
}

static inline void signal_cleanup_handler(void)
{
    running_thread->stack_top = (struct stack *)running_thread->stack_top_preserved;
}

static inline void thread_join_handler(void)
{
    /* wake up the thread waiting for join */
    if(!list_is_empty(&running_thread->join_list)) {
        wake_up(&running_thread->join_list);
    }

    /* remove the thread from the system */
    list_remove(&running_thread->thread_list);
    list_remove(&running_thread->task_list);
    list_remove(&running_thread->list);
    running_thread->status = THREAD_TERMINATED;
    bitmap_clear_bit(bitmap_threads, running_thread->tid);

    /* free the stack memory */
    free_pages((uint32_t)running_thread->stack,
               size_to_page_order(running_thread->stack_size));

    /* remove the task from the system if it contains no thread anymore */
    struct task_struct *task = running_thread->task;
    if(list_is_empty(&task->threads_list)) {
        list_remove(&task->list);
        bitmap_clear_bit(bitmap_tasks, task->pid);
    }
}

void sys_procstat(void)
{
    /* read syscall arguments */
    struct procstat_info *info = SYSCALL_ARG(struct procstat_info *, 0);

    int i = 0;
    struct list *curr;
    list_for_each(curr, &threads_list) {
        struct thread_info *thread = list_entry(curr, struct thread_info, thread_list);
        int stack_size_word = thread->stack_size / 4;

        info[i].pid = thread->task->pid;
        info[i].priority = thread->priority;
        info[i].status = thread->status;
        info[i].privileged = thread->privilege == KERNEL_THREAD;
        info[i].stack_usage = (size_t)&thread->stack[stack_size_word] - (size_t)thread->stack_top;
        info[i].stack_size = thread->stack_size;
        strncpy(info[i].name, thread->name, TASK_NAME_LEN_MAX);

        i++;
    }

    /* return task count */
    SYSCALL_ARG(int, 0) = i;
}

void sys_setprogname(void)
{
    /* read syscall arguments */
    char *name = SYSCALL_ARG(char *, 0);

    strncpy(running_thread->name, name, TASK_NAME_LEN_MAX);
}

void sys_getprogname(void)
{
    /* return pointer to the program name */
    SYSCALL_ARG(char *, 0) = running_thread->name;
}

void sys_delay_ticks(void)
{
    /* read syscall arguments */
    uint32_t tick = SYSCALL_ARG(uint32_t, 0);

    /* reconfigure the timer for sleeping */
    running_thread->sleep_ticks = tick;

    /* put the thread into the sleep list and change the status */
    running_thread->status = THREAD_WAIT;
    list_push(&sleep_list, &(running_thread->list));

    /* pass the return value with r0 */
    SYSCALL_ARG(uint32_t, 0) = 0;
}

void sys_task_create(void)
{
    /* read syscall arguments */
    task_func_t task_func = SYSCALL_ARG(task_func_t, 0);
    int priority = SYSCALL_ARG(int, 1);
    int stack_size = SYSCALL_ARG(int, 2);

    int retval = _task_create(task_func, priority, stack_size, USER_THREAD);

    SYSCALL_ARG(int, 0) = retval;
}

void sys_sched_yield(void)
{
    /* suspend the current thread */
    prepare_to_wait(&sleep_list, &running_thread->list, THREAD_WAIT);
}

void sys__exit(void)
{
    /* obtain the task of the running thread */
    struct task_struct *task = running_thread->task;

    /* remove all threads of the task from the system */
    struct list *curr, *next;
    list_for_each_safe(curr, next, &task->threads_list) {
        struct thread_info *thread = list_entry(curr, struct thread_info, task_list);

        /* remove thread from the system */
        list_remove(&thread->thread_list);
        list_remove(&thread->task_list);
        list_remove(&thread->list);
        thread->status = THREAD_TERMINATED;
        bitmap_clear_bit(bitmap_threads, thread->tid);

        /* free the stack memory */
        free_pages((uint32_t)thread->stack, size_to_page_order(thread->stack_size));
    }

    /* remove the task from the system if it contains no thread anymore */
    if(list_is_empty(&task->threads_list)) {
        list_remove(&task->list);
        bitmap_clear_bit(bitmap_tasks, task->pid);
    }
}

void sys_mount(void)
{
    /* read syscall arguments */
    char *source = SYSCALL_ARG(char *, 0);
    char *target = SYSCALL_ARG(char *, 1);

    int tid = running_thread->tid;

    /* send mount request to the file system daemon */
    if(running_thread->syscall_pending == false) {
        request_mount(tid, source, target);
    }

    /* read mount result from the file system daemon */
    int mnt_result;
    int retval = fifo_read(files[tid], (char *)&mnt_result,
                           sizeof(mnt_result), 0);

    /* check if the syscall need to be restarted */
    if(retval == -ERESTARTSYS) {
        /* file system has not finished the request yet */
        set_syscall_pending(running_thread);
    } else {
        /* request is complete */
        reset_syscall_pending(running_thread);
        SYSCALL_ARG(int, 0) = retval;
    }
}

void sys_open(void)
{
    /* read syscall argument */
    char *pathname = SYSCALL_ARG(char *, 0);
    int flags = SYSCALL_ARG(int, 1);

    int tid = running_thread->tid;
    struct task_struct *task = running_thread->task;

    /* send file open request to the file system daemon */
    if(running_thread->syscall_pending == false) {
        request_open_file(tid, pathname);
    }

    /* read the file index from the file system daemon */
    int file_idx;
    int retval = fifo_read(files[tid], (char *)&file_idx, sizeof(file_idx), 0);

    /* check if the syscall need to be restarted */
    if(retval == -ERESTARTSYS) {
        /* file system daemon has not finished the request yet */
        set_syscall_pending(running_thread);
        return;
    } else {
        /* request is complete */
        reset_syscall_pending(running_thread);
    }

    if(file_idx == -1 || task->fd_cnt >= FILE_DESC_CNT_MAX) {
        /* return on error */
        SYSCALL_ARG(int, 0) = -1;
        return;
    }

    /* find a free file descriptor entry on the table */
    int fdesc_idx = -1;
    for(int i = 0; i < FILE_DESC_CNT_MAX; i++) {
        if(task->fdtable[i].used == false) {
            fdesc_idx = i;
            break;
        }
    }

    /* unexpeceted error */
    if(fdesc_idx == -1) {
        /* return on error */
        SYSCALL_ARG(int, 0) = -1;
        return;
    }

    struct file *filp = files[file_idx];

    /* register new file descriptor to the task */
    struct fdtable *fdesc = &task->fdtable[fdesc_idx];
    fdesc->file = filp;
    fdesc->flags = flags;
    fdesc->used = true;

    /* increase file descriptor count of the task */
    task->fd_cnt++;

    /* check if the file operation is undefined */
    if(!filp->f_op->open) {
        /* return on error */
        SYSCALL_ARG(int, 0)= -ENXIO;
        return;
    }

    /* call open operation  */
    filp->f_op->open(filp->f_inode, filp);

    /* return the file descriptor */
    int fd = fdesc_idx + TASK_CNT_MAX;
    SYSCALL_ARG(int, 0) = fd;
}

void sys_close(void)
{
    /* read syscall arguments */
    int fd = SYSCALL_ARG(int, 0);

    /* a valid file descriptor number should starts from TASK_CNT_MAX */
    if(fd < TASK_CNT_MAX) {
        SYSCALL_ARG(long, 0) = EBADF;
        return;
    }

    /* calculate the index number of the file descriptor on the table */
    int fdesc_idx = fd - TASK_CNT_MAX;

    /* obtain the task from the thread */
    struct task_struct *task = running_thread->task;

    /* check if the file descriptor is indeed marked as used */
    if((task->fdtable[fdesc_idx].used != true)) {
        SYSCALL_ARG(long, 0) = EBADF;
        return;
    }

    /* free the file descriptor */
    task->fdtable[fdesc_idx].used = false;
    task->fd_cnt--;

    /* return on success */
    SYSCALL_ARG(long, 0) = 0;
}

void sys_read(void)
{
    /* read syscall argument */
    int fd = SYSCALL_ARG(int, 0);
    char *buf = SYSCALL_ARG(char *, 1);
    size_t count = SYSCALL_ARG(size_t, 2);

    /* get the file pointer */
    struct file *filp;
    if(fd < TASK_CNT_MAX) {
        /* anonymous pipe hold by each tasks */
        filp = files[fd];
    } else {
        /* obtain the task from the thread */
        struct task_struct *task = running_thread->task;

        int fdesc_idx = fd - TASK_CNT_MAX;
        filp = task->fdtable[fdesc_idx].file;
        filp->f_flags = task->fdtable[fdesc_idx].flags;
    }

    /* check if the file operation is undefined */
    if(!filp->f_op->read) {
        /* return on error */
        SYSCALL_ARG(int, 0)= -ENXIO;
        return;
    }

    /* call read operation */
    ssize_t retval = filp->f_op->read(filp, buf, count, 0);

    /* check if the syscall need to be restarted */
    if(retval == -ERESTARTSYS) {
        /* syscall need to be restarted */
        set_syscall_pending(running_thread);
    } else {
        /* syscall is complete, return the result */
        reset_syscall_pending(running_thread);
        SYSCALL_ARG(int, 0) = retval;
    }
}

void sys_write(void)
{
    /* read syscall arguments */
    int fd = SYSCALL_ARG(int, 0);
    char *buf = SYSCALL_ARG(char *, 1);
    size_t count = SYSCALL_ARG(size_t, 2);

    /* get the file pointer */
    struct file *filp;
    if(fd < TASK_CNT_MAX) {
        /* anonymous pipe hold by each tasks */
        filp = files[fd];
    } else {
        /* obtain the task from the thread */
        struct task_struct *task = running_thread->task;

        int fdesc_idx = fd - TASK_CNT_MAX;
        filp = task->fdtable[fdesc_idx].file;
        filp->f_flags = task->fdtable[fdesc_idx].flags;
    }

    /* check if the file operation is undefined */
    if(!filp->f_op->write) {
        /* return on error */
        SYSCALL_ARG(int, 0)= -ENXIO;
        return;
    }

    /* call write operation */
    ssize_t retval = filp->f_op->write(filp, buf, count, 0);

    /* check if the syscall need to be restarted */
    if(retval == -ERESTARTSYS) {
        /* syscall need to be restarted */
        set_syscall_pending(running_thread);
    } else {
        /* syscall is complete, return the result */
        reset_syscall_pending(running_thread);
        SYSCALL_ARG(int, 0) = retval;
    }
}

void sys_ioctl(void)
{
    /* read syscall arguments */
    int fd = SYSCALL_ARG(int, 0);
    unsigned int cmd = SYSCALL_ARG(unsigned int, 1);
    unsigned long arg = SYSCALL_ARG(unsigned long, 2);

    /* get the file pointer */
    struct file *filp;
    if(fd < TASK_CNT_MAX) {
        /* anonymous pipe hold by each tasks */
        filp = files[fd];
    } else {
        /* obtain the task from the thread */
        struct task_struct *task = running_thread->task;

        int fdesc_idx = fd - TASK_CNT_MAX;
        filp = task->fdtable[fdesc_idx].file;
    }

    /* check if the file operation is undefined */
    if(!filp->f_op->ioctl) {
        /* return on error */
        SYSCALL_ARG(int, 0)= -ENXIO;
        return;
    }

    /* call ioctl operation */
    int retval = filp->f_op->ioctl(filp, cmd, arg);

    /* check if the syscall need to be restarted */
    if(retval == -ERESTARTSYS) {
        /* syscall need to be restarted */
        set_syscall_pending(running_thread);
    } else {
        /* syscall is complete, return the result */
        reset_syscall_pending(running_thread);
        SYSCALL_ARG(int, 0) = retval;
    }
}

void sys_lseek(void)
{
    /* read syscall arguments */
    int fd = SYSCALL_ARG(int, 0);
    long offset = SYSCALL_ARG(long, 1);
    int whence = SYSCALL_ARG(int, 2);

    if(fd < TASK_CNT_MAX) {
        SYSCALL_ARG(long, 0) = EBADF;
        return;
    }

    /* obtain the task from the thread */
    struct task_struct *task = running_thread->task;

    /* get the file pointer from the table */
    int fdesc_idx = fd - TASK_CNT_MAX;
    struct file *filp = task->fdtable[fdesc_idx].file;

    /* check if the file operation is undefined */
    if(!filp->f_op->lseek) {
        /* return on error */
        SYSCALL_ARG(int, 0)= -ENXIO;
        return;
    }

    /* call lseek operation */
    int retval = filp->f_op->lseek(filp, offset, whence);

    /* check if the syscall need to be restarted */
    if(retval == -ERESTARTSYS) {
        /* syscall need to be restarted */
        set_syscall_pending(running_thread);
    } else {
        /* syscall is complete, return the result */
        reset_syscall_pending(running_thread);
        SYSCALL_ARG(int, 0) = retval;
    }
}

void sys_fstat(void)
{
    /* read syscall arguments */
    int fd = SYSCALL_ARG(int, 0);
    struct stat *statbuf = SYSCALL_ARG(struct stat *, 1);

    struct task_struct *task = running_thread->task;

    if(fd < TASK_CNT_MAX) {
        SYSCALL_ARG(long, 0) = EBADF;
        return;
    }

    /* read the inode of the given file */
    int fdesc_idx = fd - TASK_CNT_MAX;
    struct inode *inode = task->fdtable[fdesc_idx].file->f_inode;

    /* check if the inode exists */
    if(inode != NULL) {
        statbuf->st_mode   = inode->i_mode;
        statbuf->st_ino    = inode->i_ino;
        statbuf->st_rdev   = inode->i_rdev;
        statbuf->st_size   = inode->i_size;
        statbuf->st_blocks = inode->i_blocks;
    }

    SYSCALL_ARG(int *, 0) = 0;
}

void sys_opendir(void)
{
    /* read syscall arguments */
    char *pathname = SYSCALL_ARG(char *, 0);
    DIR *dirp = SYSCALL_ARG(DIR *, 1);

    int tid = running_thread->tid;

    /* send directory open request to the file system daemon */
    if(running_thread->syscall_pending == false) {
        request_open_directory(tid, pathname);
    }

    /* read the directory inode from the file system daemon */
    struct inode *inode_dir;
    int retval = fifo_read(files[tid], (char *)&inode_dir, sizeof(inode_dir), 0);

    /* check if the syscall need to be restarted */
    if(retval == -ERESTARTSYS) {
        /* file system daemon has not finished the request yet */
        set_syscall_pending(running_thread);
        return;
    } else {
        /* request is complete */
        reset_syscall_pending(running_thread);
    }

    /* return the directory info */
    dirp->inode_dir = inode_dir;
    dirp->dentry_list = inode_dir->i_dentry.next;

    /* pass the return value with r0 */
    if(dirp->inode_dir == NULL) {
        /* return on error */
        SYSCALL_ARG(int, 0) = -1;
    } else {
        /* return on success */
        SYSCALL_ARG(int, 0) = 0;
    }
}

void sys_readdir(void)
{
    /* read syscall arguments */
    DIR *dirp = SYSCALL_ARG(DIR *, 0);
    struct dirent *dirent = SYSCALL_ARG(struct dirent *, 1);

    /* pass the return value with r0 */
    SYSCALL_ARG(int, 0) = (uint32_t)fs_read_dir(dirp, dirent);
}

void sys_getpid(void)
{
    /* return the task pid */
    SYSCALL_ARG(int, 0) = running_thread->task->pid;
}

void sys_gettid(void)
{
    /* return the thread id */
    SYSCALL_ARG(int, 0) = running_thread->tid;
}

void sys_mknod(void)
{
    /* read syscall arguments */
    char *pathname = SYSCALL_ARG(char *, 0);
    //mode_t mode = SYSCALL_ARG(mode_t, 1);
    dev_t dev = SYSCALL_ARG(dev_t, 2);

    int tid = running_thread->tid;

    /* send file create request to the file system daemon */
    if(running_thread->syscall_pending == false) {
        request_create_file(tid, pathname, dev);
    }

    /* read the file index from the file system daemon  */
    int file_idx;
    int retval = fifo_read(files[tid], (char *)&file_idx, sizeof(file_idx), 0);

    /* check if the syscall need to be restarted */
    if(retval == -ERESTARTSYS) {
        /* file system daemon has not finished the request yet */
        set_syscall_pending(running_thread);
        return;
    } else {
        /* request is complete */
        reset_syscall_pending(running_thread);
    }

    if(file_idx == -1) {
        /* file not found, return on error */
        SYSCALL_ARG(int, 0) = -1;
    } else {
        /* return on success */
        SYSCALL_ARG(int, 0) = 0;
    }
}

void sys_mkfifo(void)
{
    /* read syscall arguments */
    char *pathname = SYSCALL_ARG(char *, 0);
    //mode_t mode = SYSCALL_ARG(mode_t, 1);

    int tid = running_thread->tid;

    /* send file create request to the file system daemon */
    if(running_thread->syscall_pending == false) {
        request_create_file(tid, pathname, S_IFIFO);
    }

    /* read the file index from the file system daemon */
    int file_idx = 0;
    int retval = fifo_read(files[tid], (char *)&tid, sizeof(file_idx), 0);

    /* check if the syscall need to be restarted */
    if(retval == -ERESTARTSYS) {
        /* file system has not finished the request yet */
        set_syscall_pending(running_thread);
        return;
    } else {
        /* request is complete */
        reset_syscall_pending(running_thread);
    }

    if(file_idx == -1) {
        /* file not found, return on error */
        SYSCALL_ARG(int, 0) = -1;
    } else {
        /* return on success */
        SYSCALL_ARG(int, 0) = 0;
    }
}

void poll_notify(struct file *filp)
{
    /* iterate through all threads */
    struct list *thread_list;
    list_for_each(thread_list, &threads_list) {
        struct thread_info *thread = list_entry(thread_list, struct thread_info, thread_list);

        /* find all threads that are waiting for poll notification */
        struct list *file_list;
        list_for_each(file_list, &thread->poll_files_list) {
            struct file *file_iter = list_entry(file_list, struct file, list);

            if(filp == file_iter) {
                wake_up(&thread->poll_wq);
            }
        }
    }
}

void sys_poll(void)
{
    /* read syscall arguments */
    struct pollfd *fds = SYSCALL_ARG(struct pollfd *, 0);
    nfds_t nfds = SYSCALL_ARG(nfds_t, 1);
    int timeout = SYSCALL_ARG(int, 2);

    if(running_thread->syscall_pending == false) {
        /* setup deadline for poll */
        if(timeout > 0) {
            struct timespec tp;
            get_sys_time(&tp);
            time_add(&tp, 0, timeout * 1000000);
            running_thread->poll_timeout = tp;
        }

        /* initialization */
        init_waitqueue_head(&running_thread->poll_wq);
        list_init(&running_thread->poll_files_list);
        running_thread->poll_failed = false;

        /* check file events */
        struct file * filp;
        bool no_event = true;

        /* obtain the task from the thread */
        struct task_struct *task = running_thread->task;

        for(int i = 0; i < nfds; i++) {
            int fd = fds[i].fd - TASK_CNT_MAX;
            filp = task->fdtable[fd].file;

            uint32_t events = filp->f_events;
            if(~fds[i].events & events) {
                no_event = false;
            }

            /* return events */
            fds[i].revents |= events;
        }

        if(!no_event) {
            /* return on success */
            SYSCALL_ARG(int, 0) = 0;
            return;
        }

        /* no events and no timeout: return immediately */
        if(timeout == 0) {
            /* return on error */
            SYSCALL_ARG(int, 0) = -1;
            return;
        }

        /* turn on the syscall pending flag */
        set_syscall_pending(running_thread);

        /* put current thread into the wait queue */
        prepare_to_wait(&running_thread->poll_wq, &running_thread->list, THREAD_WAIT);

        /* put the thread into the poll list for monitoring timeout */
        if(timeout > 0) {
            list_push(&poll_timeout_list, &running_thread->poll_list);
        }

        /* save all pollfd into the list */
        for(int i = 0; i < nfds; i++) {
            int fd = fds[i].fd - TASK_CNT_MAX;
            filp = task->fdtable[fd].file;

            list_push(&running_thread->poll_files_list, &filp->list);
        }
    } else {
        /* turn off the syscall pending flag */
        reset_syscall_pending(running_thread);

        /* clear poll files' list head */
        list_init(&running_thread->poll_files_list);

        /* remove the thread from the poll list */
        struct list *curr;
        list_for_each(curr, &poll_timeout_list) {
            if(curr == &running_thread->poll_list) {
                list_remove(curr);
                break;
            }
        }

        if(running_thread->poll_failed) {
            /* return on error */
            SYSCALL_ARG(int, 0) = -1;
        } else {
            /* return on success */
            SYSCALL_ARG(int, 0) = 0;
        }
    }
}

void sys_mq_open(void)
{
    /* read syscall arguments */
    char *name = SYSCALL_ARG(char *, 0);
    //int oflag = SYSCALL_ARG(int, 1);
    struct mq_attr *attr = SYSCALL_ARG(struct mq_attr *, 2);

    if(mq_cnt >= MQUEUE_CNT_MAX) {
        SYSCALL_ARG(mqd_t, 0) = -1;
        return;
    }

    /* initialize the ring buffer */
    struct kfifo *pipe = kfifo_alloc(attr->mq_msgsize, attr->mq_maxmsg);

    /* register a new message queue */
    int mqdes = mq_cnt;
    mq_table[mqdes].pipe = pipe;
    mq_table[mqdes].lock = 0;
    mq_table[mqdes].attr = *attr;
    mq_table[mqdes].pipe->flags = attr->mq_flags;
    strncpy(mq_table[mqdes].name, name, FILE_NAME_LEN_MAX);
    mq_cnt++;

    /* return the message queue descriptor */
    SYSCALL_ARG(mqd_t, 0) = mqdes;
}

void sys_mq_receive(void)
{
    /* read syscall arguments */
    mqd_t mqdes = SYSCALL_ARG(mqd_t, 0);
    char *msg_ptr = SYSCALL_ARG(char *, 1);
    //size_t msg_len = SYSCALL_ARG(size_t, 2); //TODO: check len?
    //unsigned int *msg_prio = SYSCALL_ARG(unsigned int *, 3);

    /* obtain message queue */
    struct msg_queue *mq = &mq_table[mqdes];
    pipe_t *pipe = mq->pipe;

    /* read message */
    ssize_t retval = pipe_read_generic(pipe, msg_ptr, 1);

    /* check if the syscall need to be restarted */
    if(retval == -ERESTARTSYS) {
        /* syscall need to be restarted */
        set_syscall_pending(running_thread);
    } else {
        /* syscall is complete, return the result */
        reset_syscall_pending(running_thread);
        SYSCALL_ARG(int, 0) = retval;
    }
}

void sys_mq_send(void)
{
    /* read syscall arguments */
    mqd_t mqdes = SYSCALL_ARG(mqd_t, 0);
    char *msg_ptr = SYSCALL_ARG(char *, 1);
    //size_t msg_len = SYSCALL_ARG(size_t, 2); //TODO: check len?
    //unsigned int msg_prio = SYSCALL_ARG(unsigned int, 3);

    /* obtain message queue */
    struct msg_queue *mq = &mq_table[mqdes];
    pipe_t *pipe = mq->pipe;

    /* send message */
    ssize_t retval = pipe_write_generic(pipe, msg_ptr, 1);

    /* check if the syscall need to be restarted */
    if(retval == -ERESTARTSYS) {
        /* syscall need to be restarted */
        set_syscall_pending(running_thread);
    } else {
        /* syscall is complete, return the result */
        reset_syscall_pending(running_thread);
        SYSCALL_ARG(int, 0) = retval;
    }
}

void sys_pthread_create(void)
{
    typedef void *(*start_routine_t)(void *);

    /* read syscall arguments */
    pthread_t *pthread = SYSCALL_ARG(pthread_t *, 0);
    pthread_attr_t *attr = SYSCALL_ARG(pthread_attr_t *, 1);
    start_routine_t start_routine = SYSCALL_ARG(start_routine_t, 2);
    //void *arg = XXX; //TODO: read stack to get the fourth argument

    /* invalid priority setting */
    if(attr->schedparam.sched_priority < 0 ||
       attr->schedparam.sched_priority > TASK_MAX_PRIORITY) {
        /* return on error */
        SYSCALL_ARG(int, 0) = -EINVAL;
        return;
    }

    /* create new thread */
    struct thread_info *thread =
        thread_create((thread_func_t)start_routine,
                      attr->schedparam.sched_priority,
                      attr->stacksize,
                      USER_THREAD);

    strcpy(thread->name, running_thread->name);

    if(!thread) {
        /* failed to create new thread, return on error */
        SYSCALL_ARG(int, 0) = -EAGAIN;
        return;
    }

    /* set the task ownership of the thread */
    thread->task = running_thread->task;
    list_push(&running_thread->task->threads_list, &thread->task_list);

    /* return the thread id */
    *pthread = thread->tid;

    /* return on success */
    SYSCALL_ARG(int, 0) = 0;
}

void sys_pthread_join(void)
{
    /* read syscall arguments */
    pthread_t tid = SYSCALL_ARG(pthread_t, 0);
    //void **retval = SYSCALL_ARG(void **, 0); //TODO

    struct thread_info *thread = acquire_thread(tid);
    if(!thread) {
        /* return on error */
        SYSCALL_ARG(int, 0) = -ESRCH;
        return;
    }

    /* check if another thread is already waiting for join */
    if(!list_is_empty(&thread->join_list)) {
        /* return on error */
        SYSCALL_ARG(int, 0) = -EINVAL;
        return;
    }

    /* check deadlock (threads should not join on each other) */
    struct list *curr;
    list_for_each(curr, &running_thread->join_list) {
        struct thread_info *check_thread = list_entry(curr, struct thread_info, list);

        /* check if the thread is waiting to join the running thread */
        if(check_thread == thread) {
            /* deadlock identified, return on error */
            SYSCALL_ARG(int, 0) = -EDEADLK;
            return;
        }
    }

    list_push(&thread->join_list, &running_thread->list);
    running_thread->status = THREAD_WAIT;

    /* return on success after the thread is waken */
    SYSCALL_ARG(int, 0) = 0;
}

void sys_pthread_cancel(void)
{
    /* read syscall arguments */
    pthread_t tid = SYSCALL_ARG(pthread_t, 0);

    struct thread_info *thread = acquire_thread(tid);
    if(!thread) {
        /* return on error */
        SYSCALL_ARG(int, 0) = -ESRCH;
        return;
    }

    /* kthread can only be set by the kernel */
    if(thread->privilege == KERNEL_THREAD) {
        /* return on error */
        SYSCALL_ARG(int, 0) = -EPERM;
        return;
    }

    thread_delete(thread);

    /* return on success */
    SYSCALL_ARG(int, 0) = 0;
}

void sys_pthread_setschedparam(void)
{
    /* read syscall arguments */
    pthread_t tid = SYSCALL_ARG(pthread_t, 0);
    //int policy = SYSCALL_ARG(int, 1);
    struct sched_param *param = SYSCALL_ARG(struct sched_param *, 2);

    struct thread_info *thread = acquire_thread(tid);
    if(!thread) {
        /* return on error */
        SYSCALL_ARG(int, 0) = -ESRCH;
        return;
    }

    /* kthread can only be set by the kernel */
    if(thread->privilege == KERNEL_THREAD) {
        /* return on error */
        SYSCALL_ARG(int, 0) = -EPERM;
        return;
    }

    /* invalid priority parameter */
    if(param->sched_priority < 0 ||
       param->sched_priority > TASK_MAX_PRIORITY) {
        /* return on error */
        SYSCALL_ARG(int, 0) = -EINVAL;
        return;
    }

    /* apply settings */
    thread->priority = param->sched_priority;

    /* return on success */
    SYSCALL_ARG(int, 0) = 0;
}

void sys_pthread_getschedparam(void)
{
    /* read syscall arguments */
    pthread_t tid = SYSCALL_ARG(pthread_t, 0);
    //int *policy = SYSCALL_ARG(int *, 1);
    struct sched_param *param = SYSCALL_ARG(struct sched_param *, 2);

    struct thread_info *thread = acquire_thread(tid);
    if(!thread) {
        /* return on error */
        SYSCALL_ARG(int, 0) = -ESRCH;
        return;
    }

    /* kthread can only be set by the kernel */
    if(thread->privilege == KERNEL_THREAD) {
        /* return on error */
        SYSCALL_ARG(int, 0) = -EPERM;
        return;
    }

    /* return settings */
    param->sched_priority = thread->priority;

    /* return on success */
    SYSCALL_ARG(int, 0) = 0;
}

void sys_pthread_yield(void)
{
    /* suspend the current thread */
    prepare_to_wait(&sleep_list, &running_thread->list, THREAD_WAIT);
}

static void handle_signal(struct thread_info *thread, int signum)
{
    bool stage_handler = false;

    /* wake up the thread from the signal waiting list */
    if(thread->wait_for_signal && (sig2bit(signum) & thread->sig_wait_set)) {
        /* disable the signal waiting flag */
        thread->wait_for_signal = false;

        /* wake up the waiting thread and setup return values */
        finish_wait(&thread->list);
        *thread->ret_sig = signum;
        *(int *)thread->reg.r0 = 0;
    }

    switch(signum) {
        case SIGUSR1:
            stage_handler = true;
            break;
        case SIGUSR2:
            stage_handler = true;
            break;
        case SIGPOLL:
            stage_handler = true;
            break;
        case SIGSTOP:
            /* stop: can't be caught or ignored */
            thread_suspend(thread);
            break;
        case SIGCONT:
            /* continue */
            thread_resume(thread);
            stage_handler = true;
            break;
        case SIGKILL: {
            /* kill: can't be caught or ignored */
            thread_delete(thread);
            break;
        }
    }

    if(!stage_handler) {
        return;
    }

    int sig_idx = get_signal_index(signum);
    struct sigaction *act = thread->sig_table[sig_idx];

    /* no signal handler is provided */
    if(act == NULL) {
        return;
    } else if(act->sa_handler == NULL) {
        return;
    }

    /* stage the signal or sigaction handler */
    if(act->sa_flags & SA_SIGINFO) {
        stage_sigaction_handler(thread, act->sa_sigaction,
                                signum, NULL, NULL);
    } else {
        stage_signal_handler(thread, act->sa_handler, signum);
    }
}

void sys_pthread_kill(void)
{
    /* read syscall arguments */
    pthread_t tid = SYSCALL_ARG(pthread_t, 0);
    int sig = SYSCALL_ARG(int, 1);

    struct thread_info *thread = acquire_thread(tid);

    /* failed to find the task with the pid */
    if(!thread) {
        /* return on error */
        SYSCALL_ARG(int, 0) = -ESRCH;
        return;
    }

    /* kernel thread does not support posix signals */
    if(thread->privilege == KERNEL_THREAD) {
        /* return on error */
        SYSCALL_ARG(int, 0) = -EPERM;
        return;
    }

    /* check if the signal number is defined */
    if(!is_signal_defined(sig)) {
        /* return on error */
        SYSCALL_ARG(int, 0) = -EINVAL;
        return;
    }

    handle_signal(thread, sig);

    /* return on success */
    SYSCALL_ARG(int, 0) = 0;
}

void sys_pthread_exit(void)
{
    //TODO: handle return value pointer

    thread_join_handler();
}

void sys_pthread_mutex_unlock(void)
{
    /* read syscall arguments */
    pthread_mutex_t *mutex = SYSCALL_ARG(pthread_mutex_t *, 0);

    /* only the owner thread can unlock the mutex */
    if(mutex->owner != running_thread) {
        /* return on error */
        SYSCALL_ARG(int, 0) = EPERM;
    } else {
        /* release the mutex */
        mutex->owner = NULL;

        /* check the mutex waiting list */
        if(!list_is_empty(&mutex->wait_list)) {
            /* wake up a thread from the mutex waiting list */
            wake_up(&mutex->wait_list);
        }

        /* return on success */
        SYSCALL_ARG(int, 0) = 0;
    }
}

void sys_pthread_mutex_lock(void)
{
    /* read syscall arguments */
    pthread_mutex_t *mutex = SYSCALL_ARG(pthread_mutex_t *, 0);

    /* check if mutex is already occupied */
    if(mutex->owner != NULL) {
        /* put the current thread into the mutex waiting list */
        prepare_to_wait(&mutex->wait_list, &running_thread->list, THREAD_WAIT);

        /* turn on the syscall pending flag */
        set_syscall_pending(running_thread);
    } else {
        /* occupy the mutex by setting the owner */
        mutex->owner = running_thread;

        /* turn off the syscall pending flag */
        reset_syscall_pending(running_thread);

        /* return on success */
        SYSCALL_ARG(int, 0) = 0;
    }
}

void sys_pthread_cond_signal(void)
{
    /* read syscall arguments */
    pthread_cond_t *cond = SYSCALL_ARG(pthread_cond_t*, 0);

    /* wake up a thread from the wait list */
    if(!list_is_empty(&cond->task_wait_list)) {
        wake_up(&cond->task_wait_list);
    }

    /* pthread_cond_signal never returns error code */
    SYSCALL_ARG(int, 0) = 0;
}

void sys_pthread_cond_wait(void)
{
    /* read syscall arguments */
    pthread_cond_t *cond = SYSCALL_ARG(pthread_cond_t *, 0);
    pthread_mutex_t *mutex = SYSCALL_ARG(pthread_mutex_t *, 1);

    if(mutex->owner) {
        /* release the mutex */
        mutex->owner = NULL;

        /* put the current thread into the read waiting list */
        prepare_to_wait(&cond->task_wait_list, &running_thread->list, THREAD_WAIT);
    }

    /* pthread_cond_wait never returns error code */
    SYSCALL_ARG(int, 0) = 0;
}

void sys_pthread_once(void)
{
    //typedef void (*init_routine_t)(void);

    /* read syscall arguments */
    //pthread_once_t *once_control = SYSCALL_ARG(pthread_once_t *, 0);
    //init_routine_t init_routine = SYSCALL_ARG(init_routine_t, 0);
}

void sys_sem_post(void)
{
    /* read syscall arguments */
    sem_t *sem = SYSCALL_ARG(sem_t *, 0);

    /* prevent integer overflow */
    if(sem->count >= (INT32_MAX - 1)) {
        /* turn on the syscall pending flag */
        set_syscall_pending(running_thread);
    } else {
        /* increase the semaphore */
        sem->count++;

        /* wake up a thread from the waiting list */
        if(sem->count > 0 && !list_is_empty(&sem->wait_list)) {
            wake_up(&sem->wait_list);
        }

        /* turn off the syscall pending flag */
        reset_syscall_pending(running_thread);

        /* return on success */
        SYSCALL_ARG(int, 0) = 0;
    }
}

void sys_sem_trywait(void)
{
    /* read syscall arguments */
    sem_t *sem = SYSCALL_ARG(sem_t *, 0);

    if(sem->count <= 0) {
        /* failed to obtain the semaphore, put the current thread into the waiting list */
        prepare_to_wait(&sem->wait_list, &running_thread->list, THREAD_WAIT);

        /* return on error */
        SYSCALL_ARG(int, 0) = -EAGAIN;
    } else {
        /* successfully obtained the semaphore */
        sem->count--;

        /* return on success */
        SYSCALL_ARG(int, 0) = 0;
    }
}

void sys_sem_wait(void)
{
    /* read syscall arguments */
    sem_t *sem = SYSCALL_ARG(sem_t *, 0);

    if(sem->count <= 0) {
        /* failed to obtain the semaphore, put the current thread into the waiting list */
        prepare_to_wait(&sem->wait_list, &running_thread->list, THREAD_WAIT);

        /* turn on the syscall pending flag */
        set_syscall_pending(running_thread);
    } else {
        /* successfully obtained the semaphore */
        sem->count--;

        /* turn off the syscall pending flag */
        reset_syscall_pending(running_thread);

        /* return on success */
        SYSCALL_ARG(int, 0) = 0;
    }
}

void sys_sem_getvalue(void)
{
    /* read syscall arguments */
    sem_t *sem = SYSCALL_ARG(sem_t *, 0);
    int *sval = SYSCALL_ARG(int *, 0);

    *sval = sem->count;

    /* return on success */
    SYSCALL_ARG(int, 0) = 0;
}

void sys_sigaction(void)
{
    /* read syscall arguments */
    int signum = SYSCALL_ARG(int, 0);
    struct sigaction *act = SYSCALL_ARG(struct sigaction *, 1);
    struct sigaction *oldact = SYSCALL_ARG(struct sigaction *, 2);

    /* check if the signal number is defined */
    if(!is_signal_defined(signum)) {
        /* return on error */
        SYSCALL_ARG(int, 0) = -EINVAL;
        return;
    }

    /* get the address of the signal action on the table */
    int sig_idx = get_signal_index(signum);
    struct sigaction *sig_entry = running_thread->sig_table[sig_idx];

    /* has the signal action already been registed on the table? */
    if(sig_entry) {
        /* preserve old signal action */
        if(oldact) {
            *oldact = *sig_entry;
        }

        /* replace old signal action */
        *sig_entry = *act;
    } else {
        /* allocate memory for new action */
        struct sigaction *new_act = kmalloc(sizeof(act));

        /* failed to allocate new memory */
        if(new_act == NULL) {
            /* return on error */
            SYSCALL_ARG(int, 0) = -ENOMEM;
            return;
        }

        /* register the signal action on the table */
        running_thread->sig_table[sig_idx] = new_act;
        *new_act = *act;
    }

    /* return on success */
    SYSCALL_ARG(int, 0) = 0;
}

void sys_sigwait(void)
{
    /* read syscall arguments */
    sigset_t *set = SYSCALL_ARG(sigset_t *, 0);
    int *sig = SYSCALL_ARG(int *, 1);

    sigset_t invalid_mask = ~(sig2bit(SIGUSR1) | sig2bit(SIGUSR2) |
                              sig2bit(SIGPOLL) | sig2bit(SIGSTOP) |
                              sig2bit(SIGCONT) | sig2bit(SIGKILL));

    /* reject request of waiting on undefined signal */
    if(*set & invalid_mask) {
        /* return on error */
        SYSCALL_ARG(int, 0) = -EINVAL;
        return;
    }

    /* record the signal set to wait */
    running_thread->sig_wait_set = *set;
    running_thread->ret_sig = sig;
    running_thread->wait_for_signal = true;

    /* put the thread into the signal waiting list */
    prepare_to_wait(&signal_wait_list, &running_thread->list, THREAD_WAIT);
}

void sys_kill(void)
{
    /* read syscall arguments */
    pid_t pid = SYSCALL_ARG(pid_t, 0);
    int sig = SYSCALL_ARG(int, 1);

    struct task_struct *task = acquire_task(pid);

    /* failed to find the task with the pid */
    if(!task) {
        /* return on error */
        SYSCALL_ARG(int, 0) = -ESRCH;
        return;
    }

    /* check if the signal number is defined */
    if(!is_signal_defined(sig)) {
        /* return on error */
        SYSCALL_ARG(int, 0) = -EINVAL;
        return;
    }

    struct list *curr;
    list_for_each(curr, &task->threads_list) {
        struct thread_info *thread = list_entry(curr, struct thread_info, task_list);
        handle_signal(thread, sig);
    }

    /* return on success */
    SYSCALL_ARG(int, 0) = 0;
}

void sys_clock_gettime(void)
{
    /* read syscall arguments */
    clockid_t clockid = SYSCALL_ARG(clockid_t, 0);
    struct timespec *tp = SYSCALL_ARG(struct timespec *, 1);

    if(clockid != CLOCK_MONOTONIC) {
        /* return on error */
        SYSCALL_ARG(int, 0) = -EINVAL;
        return;
    }

    get_sys_time(tp);

    /* return on success */
    SYSCALL_ARG(int, 0) = 0;
}

void sys_clock_settime(void)
{
    /* read syscall arguments */
    clockid_t clockid = SYSCALL_ARG(clockid_t, 0);
    struct timespec *tp = SYSCALL_ARG(struct timespec *, 1);

    if(clockid != CLOCK_REALTIME) {
        /* return on error */
        SYSCALL_ARG(int, 0) = -EINVAL;
        return;
    }

    set_sys_time(tp);

    /* return on success */
    SYSCALL_ARG(int, 0) = 0;
}

void sys_timer_create(void)
{
    /* read syscall arguments */
    clockid_t clockid = SYSCALL_ARG(clockid_t, 0);
    struct sigevent *sevp = SYSCALL_ARG(struct sigevent *, 1);
    timer_t *timerid = SYSCALL_ARG(timer_t *, 2);

    /* unsupported clock source */
    if(clockid != CLOCK_MONOTONIC) {
        /* return on error */
        SYSCALL_ARG(int, 0) = -EINVAL;
        return;
    }

    if(sevp->sigev_notify != SIGEV_NONE &&
       sevp->sigev_notify != SIGEV_SIGNAL) {
        /* return on error */
        SYSCALL_ARG(int, 0) = -EINVAL;
        return;
    }

    /* allocate memory for the new timer */
    struct timer *new_tm = kmalloc(sizeof(struct timer));

    /* failed to allocate memory */
    if(new_tm == NULL) {
        /* return on error */
        SYSCALL_ARG(int, 0) = -ENOMEM;
        return;
    }

    /* record timer settings */
    new_tm->id = running_thread->timer_cnt;
    new_tm->sev = *sevp;
    new_tm->thread = running_thread;

    /* timer list initialization */
    if(running_thread->timer_cnt == 0) {
        /* initialize the timer list head */
        list_init(&running_thread->timers_list);
    }

    /* put the new timer into the list */
    list_push(&timers_list, &new_tm->g_list);
    list_push(&running_thread->timers_list, &new_tm->list);

    /* return timer id */
    *timerid = running_thread->timer_cnt;

    /* increase timer count */
    running_thread->timer_cnt++;

    /* return on success */
    SYSCALL_ARG(int, 0) = 0;
}

void sys_timer_delete(void)
{
    /* read syscall arguments */
    timer_t timerid = SYSCALL_ARG(timer_t, 0);

    struct timer *timer;

    /* find the timer with the id */
    struct list *curr, *next;
    list_for_each_safe(curr, next, &running_thread->timers_list) {
        /* get the timer */
        timer = list_entry(curr, struct timer, list);

        /* update remained ticks of waiting */
        if(timer->id == timerid) {
            /* remove the timer from the lists and
             * free the memory */
            list_remove(&timer->g_list);
            list_remove(&timer->list);
            kfree(timer);

            /* return on success */
            SYSCALL_ARG(int, 0) = 0;
            return;
        }
    }

    /* invalid timer id, return on error */
    SYSCALL_ARG(int, 0) = -EINVAL;
}

void sys_timer_settime(void)
{
    /* read syscall arguments */
    timer_t timerid = SYSCALL_ARG(timer_t, 0);
    int flags = SYSCALL_ARG(int, 1);
    struct itimerspec *new_value = SYSCALL_ARG(struct itimerspec *, 2);
    struct itimerspec *old_value = SYSCALL_ARG(struct itimerspec *, 3);

    /* bad arguments */
    if(new_value->it_value.tv_sec < 0 ||
       new_value->it_value.tv_nsec < 0 ||
       new_value->it_value.tv_nsec > 999999999) {
        /* return on error */
        SYSCALL_ARG(int, 0) = -EINVAL;
        return;
    }

    struct timer *timer;
    bool timer_found = false;

    /* the task has no timer */
    if(running_thread->timer_cnt == 0) {
        /* return on error */
        SYSCALL_ARG(int, 0) = -EINVAL;
        return;
    }

    /* find the timer with the id */
    struct list *curr;
    list_for_each(curr, &running_thread->timers_list) {
        /* get the timer */
        timer = list_entry(curr, struct timer, list);

        /* update remained ticks of waiting */
        if(timer->id == timerid) {
            timer_found = true;
            break;
        }
    }

    if(timer_found) {
        /* return old timer setting */
        if(old_value != NULL) {
            *old_value = timer->setting;
        }

        /* save timer's setting */
        timer->flags = flags;
        timer->setting = *new_value;
        timer->ret_time = timer->setting;

        /* enable the timer */
        timer->counter = timer->setting.it_value;
        timer->enabled = true;

        /* return on success */
        SYSCALL_ARG(int, 0) = 0;
    } else {
        /* return on error */
        SYSCALL_ARG(int, 0) = -EINVAL;
    }
}

void sys_timer_gettime(void)
{
    /* read syscall arguments */
    timer_t timerid = SYSCALL_ARG(timer_t, 0);
    struct itimerspec *curr_value = SYSCALL_ARG(struct itimerspec *, 1);

    struct timer *timer;
    bool found = false;

    /* find the timer with the id */
    struct list *curr;
    list_for_each(curr, &running_thread->timers_list) {
        timer = list_entry(curr, struct timer, list);

        /* check timer id */
        if(timerid == timer->id) {
            found = true;
            break;
        }
    }

    if(found) {
        /* return on success */
        *curr_value = timer->ret_time;
        SYSCALL_ARG(int, 0) = 0;
    } else {
        /* return on error */
        SYSCALL_ARG(int, 0) = -EINVAL;
    }
}

void sys_malloc(void)
{
    /* read syscall arguments */
    size_t size = SYSCALL_ARG(size_t, 0);

    SYSCALL_ARG(void *, 0) = kmalloc(size);
}

void sys_free(void)
{
    /* read syscall arguments */
    void *ptr = SYSCALL_ARG(void *, 0);

    kfree(ptr);
}

static void threads_ticks_update(void)
{
    /* update the sleep timers */
    struct list *curr;
    list_for_each(curr, &sleep_list) {
        /* get the thread info */
        struct thread_info *thread = list_entry(curr, struct thread_info, list);

        /* update remained ticks of waiting */
        if(thread->sleep_ticks > 0) {
            thread->sleep_ticks--;
        }
    }
}

static void timers_update(void)
{
    struct list *curr;
    list_for_each(curr, &timers_list) {
        struct timer *timer = list_entry(curr, struct timer, g_list);

        if(!timer->enabled) {
            continue;
        }

        /* update the timer */
        timer_down_count(&timer->counter);

        /* update the return time */
        timer->ret_time.it_value = timer->counter;

        /* check if the time is up */
        if(timer->counter.tv_sec != 0 ||
           timer->counter.tv_nsec != 0) {
            continue; /* no */
        }

        /* reload the timer */
        timer->counter = timer->setting.it_interval;

        /* shutdown the one-shot type timer */
        if(timer->setting.it_interval.tv_sec == 0 &&
           timer->setting.it_interval.tv_nsec == 0) {
            timer->enabled = false;
        }

        /* stage the signal handler */
        if(timer->sev.sigev_notify == SIGEV_SIGNAL) {
            sa_handler_t func = (sa_handler_t)timer->sev.sigev_notify_function;
            stage_signal_handler(timer->thread, func, 0);
        }
    }
}

static void poll_timeout_update(void)
{
    /* get current time */
    struct timespec tp;
    get_sys_time(&tp);

    /* iterate through all tasks that waiting for poll events */
    struct list *curr;
    list_for_each(curr, &poll_timeout_list) {
        struct thread_info *thread = list_entry(curr, struct thread_info, poll_list);

        /* wake up the thread if the time is up */
        if(tp.tv_sec >= thread->poll_timeout.tv_sec &&
           tp.tv_nsec >= thread->poll_timeout.tv_nsec) {
            thread->poll_failed = true;
            wake_up(&thread->poll_wq);
        }
    }
}

static void system_ticks_update(void)
{
    sys_time_update_handler();
    threads_ticks_update();
    timers_update();
    poll_timeout_update();
}

static void stop_running_thread(void)
{
    /* the function is triggered when the thread time quantum exhausted */
    if(running_thread->status == THREAD_RUNNING) {
        prepare_to_wait(&sleep_list, &running_thread->list, THREAD_WAIT);
    }
}

static inline bool check_systick_event(void)
{
    /* if r7 is negative, the kernel is returned from the systick
     * interrupt. otherwise the kernel is returned from the
     * supervisor call.
     */
    if((int)SUPERVISOR_EVENT(running_thread) < 0) {
        SUPERVISOR_EVENT(running_thread) *= -1; //restore to positive
        return true;
    }

    return false;
}

static inline void prepare_syscall_args(void)
{
    /*
     * the stack layouts are different according to the fpu is used or not due to the
     * lazy context switch mechanism of the arm processor. when lr[4] bit is set as 0,
     * the fpu is used otherwise it is unused.
     */
    if(running_thread->stack_top->_lr & 0x10) {
        struct stack *sp = (struct stack *)running_thread->stack_top;
        running_thread->reg.r0 = &sp->r0;
        running_thread->reg.r1 = &sp->r1;
        running_thread->reg.r2 = &sp->r2;
        running_thread->reg.r3 = &sp->r3;
    } else {
        struct stack_fpu *sp_fpu = (struct stack_fpu *)running_thread->stack_top;
        running_thread->reg.r0 = &sp_fpu->r0;
        running_thread->reg.r1 = &sp_fpu->r1;
        running_thread->reg.r2 = &sp_fpu->r2;
        running_thread->reg.r3 = &sp_fpu->r3;
    }
}

static void syscall_handler(void)
{
    for(int i = 0; i < SYSCALL_CNT; i++) {
        if(SUPERVISOR_EVENT(running_thread) == syscall_table[i].num) {
            /* execute the syscall service */
            prepare_syscall_args();
            syscall_table[i].syscall_handler();
            break;
        }
    }
}

static void supervisor_request_handler(void)
{
    if(SUPERVISOR_EVENT(running_thread) == SIGNAL_CLEANUP_EVENT) {
        signal_cleanup_handler();
    } else if(SUPERVISOR_EVENT(running_thread) == THREAD_JOIN_EVENT) {
        thread_join_handler();
    } else {
        syscall_handler();
    }
}

static void schedule(void)
{
    /* the time quantum of the current thread has not exhausted yet */
    if(running_thread->status == THREAD_RUNNING)
        return;

    /* awaken sleep threads if the sleep tick exhausted */
    struct list *curr, *next;
    list_for_each_safe(curr, next, &sleep_list) {
        /* obtain the thread info */
        struct thread_info *thread = list_entry(curr, struct thread_info, list);

        /* move the thread into the ready list if it is ready */
        if(thread->sleep_ticks == 0) {
            list_move(curr, &ready_list[thread->priority]);
            thread->status = THREAD_READY;
        }
    }

    /* find a ready list that contains runnable tasks */
    int pri;
    for(pri = TASK_MAX_PRIORITY; pri >= 0; pri--) {
        if(list_is_empty(&ready_list[pri]) == false)
            break;
    }

    /* select a task from the ready list */
    struct list *new_thread_l = list_pop(&ready_list[pri]);
    running_thread = list_entry(new_thread_l, struct thread_info, list);
    running_thread->status = THREAD_RUNNING;
}

static void print_platform_info(void)
{
    printk("Tenok RTOS (built time: %s %s)", __TIME__, __DATE__);
    printk("Machine model: %s", __BOARD_NAME__);
}

void first(void)
{
    setprogname("idle");

    /* platform initialization */
    print_platform_info();
    __platform_init();
    rom_dev_init();

    /* mount rom file system */
    mount("/dev/rom", "/");

    /* launched all hooked user tasks */
    extern char _tasks_start;
    extern char _tasks_end;

    int func_list_size = ((uint8_t *)&_tasks_end - (uint8_t *)&_tasks_start);
    int hook_task_cnt = func_list_size / sizeof(struct task_hook);

    struct task_hook *hook_task = (struct task_hook *)&_tasks_start;

    for(int i = 0; i < hook_task_cnt; i++) {
        task_create(hook_task[i].task_func,
                    hook_task[i].priority,
                    hook_task[i].stacksize);
    }

    /* idle loop with lowest thread priority */
    while(1);
}

void sched_start(void)
{
    /* initialize the irq vector table */
    irq_init();

    uint32_t stack_empty[32]; //a dummy stack for os enviromnent initialization
    os_env_init((uint32_t)(stack_empty + 32) /* point to the top */);

    /* initialize the memory pool */
    memory_pool_init(&mem_pool, mem_pool_buf, MEM_POOL_SIZE);

    /* initialize fifos for all threads */
    for(int i = 0; i < TASK_CNT_MAX; i++) {
        fifo_init(i, (struct file **)&files, NULL);
    }

    /* list initializations */
    list_init(&tasks_list);
    list_init(&threads_list);
    list_init(&sleep_list);
    list_init(&suspend_list);
    list_init(&timers_list);
    list_init(&signal_wait_list);
    list_init(&poll_timeout_list);
    for(int i = 0; i <= TASK_MAX_PRIORITY; i++) {
        list_init(&ready_list[i]);
    }

    rootfs_init();
    printk_init();

    /* initialize kernel threads and deamons */
    kthread_create(first, 0, 512);
    kthread_create(file_system_task, 1, 1024);
    softirq_init();

    /* manually set task 0 as the first thread to run */
    running_thread = &threads[0];
    threads[0].status = THREAD_RUNNING;
    list_remove(&threads[0].list); //remove from the sleep list

    while(1) {
        if(check_systick_event()) {
            system_ticks_update();
            stop_running_thread();
        } else {
            supervisor_request_handler();
        }

        /* select new thread */
        schedule();

        /* a thread is awakened to complete the pending syscall */
        if(running_thread->syscall_pending) {
            syscall_handler();
            schedule();
        }

        printk_handler();

        /* jump to the selected thread */
        running_thread->stack_top =
            (struct stack *)jump_to_thread((uint32_t)running_thread->stack_top,
                                           running_thread->privilege);
    }
}
