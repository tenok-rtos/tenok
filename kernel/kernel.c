#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include <fs/fs.h>
#include <mm/mpool.h>
#include <kernel/list.h>
#include <kernel/pipe.h>
#include <kernel/fifo.h>
#include <kernel/kernel.h>
#include <kernel/syscall.h>

#include <tenok/time.h>
#include <tenok/mqueue.h>
#include <tenok/unistd.h>
#include <tenok/pthread.h>
#include <tenok/semaphore.h>
#include <tenok/sys/stat.h>
#include <tenok/sys/mount.h>

#include "kconfig.h"
#include "stm32f4xx.h"

#define HANDLER_MSP  0xFFFFFFF1
#define THREAD_MSP   0xFFFFFFF9
#define THREAD_PSP   0xFFFFFFFD

#define INITIAL_XPSR 0x01000000

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

/* posix message queue */
struct msg_queue mq_table[MQUEUE_CNT_MAX];
int mq_cnt = 0;

bool irq_off = false;

//XXX
int hook_task_cnt = 0;
int curr_hook_task = 0;
task_func_t *hook_task_func = NULL;

/* syscall table */
syscall_info_t syscall_table[] = {
    SYSCALL_TABLE_INIT
};

int syscall_table_size = sizeof(syscall_table) / sizeof(syscall_info_t);

void *kmalloc(size_t size)
{
    return memory_pool_alloc(&mem_pool, size);
}

struct task_ctrl_blk *current_task_info(void)
{
    return running_task;
}

void task_return_handler(void)
{
    /*
     * the task function is returned. the user can defined the behavior by modifying
     * the return handler function. by default the os treats this as an error and disable
     * the interrupts then enter into the infinite loop.
     */
    asm volatile ("cpsid i\n"
                  "cpsid f\n");

    while(1);
}

void task_create(task_func_t task_func, uint8_t priority)
{
    if(task_cnt > TASK_CNT_MAX) {
        return;
    }

    tasks[task_cnt].pid = task_cnt;
    tasks[task_cnt].status = TASK_WAIT;
    tasks[task_cnt].priority = priority;
    tasks[task_cnt].stack_size = TASK_STACK_SIZE; //TODO: variable size?
    tasks[task_cnt].fd_cnt = 0;
    for(int i = 0; i < FILE_DESC_CNT_MAX; i++) {
        tasks[task_cnt].fdtable[i].used = false;
    }

    /*
     * stack design contains three parts:
     * xpsr, pc, lr, r12, r3, r2, r1, r0, (for setup exception return),
     * _r7 (for passing system call number), and
     * _lr, r11, r10, r9, r8, r7, r6, r5, r4 (for context switch)
     */
    uint32_t *stack_top = tasks[task_cnt].stack + tasks[task_cnt].stack_size - 18;
    stack_top[17] = INITIAL_XPSR;
    stack_top[16] = (uint32_t)task_func;           // pc = task_entry
    stack_top[15] = (uint32_t)task_return_handler; // lr
    stack_top[8]  = THREAD_PSP;                    //_lr = 0xfffffffd
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

void wait_event(struct list *wq, bool condition)
{
    CURRENT_TASK_INFO(curr_task);

    if(condition) {
        curr_task->syscall_pending = false;
    } else {
        prepare_to_wait(wq, &curr_task->list, TASK_WAIT);
        curr_task->syscall_pending = true;
    }
}

/*
 * move the task from the wait_list into the ready_list.
 * the task will not be executed immediately and requires
 * to wait until the scheduler select it.
 */
void wake_up(struct list *wait_list)
{
    if(list_is_empty(wait_list))
        return;

    /* pop the task from the waiting list */
    struct list *waken_task_list = list_pop(wait_list);
    struct task_ctrl_blk *waken_task = list_entry(waken_task_list, struct task_ctrl_blk, list);

    /* put the task into the ready list */
    list_push(&ready_list[waken_task->priority], &waken_task->list);
    waken_task->status = TASK_READY;
}

void schedule(void)
{
    /* since certain os functions may disable the irq with syscall, the os should not reschedule
     * the task until the irq is re-enable the task again */
    if(irq_off == true)
        return;

    /* awake the sleep tasks if the tick is exhausted */
    struct list *list_itr = sleep_list.next;
    while(list_itr != &sleep_list) {
        /* since the task may be removed from the list, the next task must be recorded first */
        struct list *next = list_itr->next;

        /* obtain the task control block */
        struct task_ctrl_blk *task = list_entry(list_itr, struct task_ctrl_blk, list);

        /* task is ready, push it into the ready list by its priority */
        if(task->sleep_ticks == 0) {
            task->status = TASK_READY;
            list_remove(list_itr); //remove the task from the sleep list
            list_push(&ready_list[task->priority], list_itr);
        }

        /* prepare the next task to check */
        list_itr = next;
    }

    /* find a ready list that contains runnable tasks */
    int pri;
    for(pri = TASK_MAX_PRIORITY; pri >= 0; pri--) {
        if(list_is_empty(&ready_list[pri]) == false)
            break;
    }

    /* task returned to the kernel before the time quantum is exhausted */
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

static void tasks_tick_update(void)
{
    sys_time_update_handler();

    /* the time quantum for the task is exhausted and require re-scheduling */
    if(running_task->status == TASK_RUNNING) {
        prepare_to_wait(&sleep_list, &running_task->list, TASK_WAIT);
    }

    /* update the sleep timers */
    struct list *curr;
    list_for_each(curr, &sleep_list) {
        /* get the task control block */
        struct task_ctrl_blk *task = list_entry(curr, struct task_ctrl_blk, list);

        /* update remained ticks of waiting */
        if(task->sleep_ticks > 0) {
            task->sleep_ticks--;
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

void sys_set_irq(void)
{
    /* read syscall arguments */
    uint32_t state = SYSCALL_ARG(uint32_t, 0);

    if(state) {
        //asm volatile ("cpsie i"); //enable all irq
        reset_basepri();
        irq_off = false;
    } else {
        //asm volatile ("cpsid i"); //disable all irq
        set_basepri();
        irq_off = true;
    }
}

void sys_set_program_name(void)
{
    /* read syscall arguments */
    char *name = SYSCALL_ARG(char *, 0);

    strncpy(running_task->name, name, TASK_NAME_LEN_MAX);
}

void sys_delay_ticks(void)
{
    /* read syscall arguments */
    uint32_t tick = SYSCALL_ARG(uint32_t, 0);

    /* reconfigure the timer for sleeping */
    running_task->sleep_ticks = tick;

    /* put the task into the sleep list and change the status */
    running_task->status = TASK_WAIT;
    list_push(&sleep_list, &(running_task->list));

    /* pass the return value with r0 */
    SYSCALL_ARG(uint32_t, 0) = 0;
}

void sys_sched_yield(void)
{
    /* suspend the current task */
    prepare_to_wait(&sleep_list, &running_task->list, TASK_WAIT);
}

void sys_fork(void)
{
    if(task_cnt > TASK_CNT_MAX) {
        /* return on error */
        SYSCALL_ARG(int, 0) = -1;
        return;
    }

    /* calculate the used space of the parent task's stack */
    uint32_t *parent_stack_end = running_task->stack + running_task->stack_size;
    uint32_t stack_used = parent_stack_end - (uint32_t *)running_task->stack_top;

    /* set priority, the priority must be higher than the idle task! */
    tasks[task_cnt].priority = (running_task->priority == 0) ? TASK_PRIORITY_MIN : running_task->priority;

    tasks[task_cnt].pid = task_cnt;
    tasks[task_cnt].stack_size = running_task->stack_size;
    tasks[task_cnt].stack_top = (struct task_stack *)(tasks[task_cnt].stack + tasks[task_cnt].stack_size - stack_used);

    /* put the forked task into the sleep list */
    tasks[task_cnt].status = TASK_WAIT;
    list_push(&sleep_list, &tasks[task_cnt].list);

    /* copy the stack of the used part only */
    memcpy(tasks[task_cnt].stack_top, running_task->stack_top, sizeof(uint32_t)*stack_used);

    /* return the child task pid to the parent task */
    SYSCALL_ARG(int, 0) = task_cnt;

    /*
     * select the proper stack layout and return the pid number to the child task
     * check lr[4] bits (0: fpu is used, 1: fpu is unused)
     */
    if(running_task->stack_top->_lr & 0x10) {
        struct task_stack *sp = (struct task_stack *)tasks[task_cnt].stack_top;
        sp->r0 = 0; //return 0 to the child task
    } else {
        struct task_stack_fpu *sp_fpu = (struct task_stack_fpu *)tasks[task_cnt].stack_top;
        sp_fpu->r0 = 0; //return 0 to the child task

    }

    task_cnt++;
}

void sys_mount(void)
{
    /* read syscall arguments */
    char *source = SYSCALL_ARG(char *, 0);
    char *target = SYSCALL_ARG(char *, 1);

    int pid = running_task->pid;

    /* if the pending flag is set that means the request is already sent */
    if(running_task->syscall_pending == false) {
        request_mount(pid, source, target);
    }

    /* receive the mount result */
    int result;
    if(fifo_read(files[pid], (char *)&result, sizeof(result), 0) == sizeof(result)) {
        //return the file descriptor number with r0
        SYSCALL_ARG(int, 0) = result;
    }
}

void sys_open(void)
{
    /* read syscall argument */
    char *pathname = SYSCALL_ARG(char *, 0);
    int flags = SYSCALL_ARG(int, 1);

    int pid = running_task->pid;

    /* if the pending flag is set that means the request is already sent */
    if(running_task->syscall_pending == false) {
        request_open_file(pid, pathname);
    }

    /* inquire the file descriptor from the file system task */
    int file_idx;
    if(fifo_read(files[pid], (char *)&file_idx, sizeof(file_idx), 0) == sizeof(file_idx)) {
        if(file_idx == -1 || running_task->fd_cnt >= FILE_DESC_CNT_MAX) {
            /* return on error */
            SYSCALL_ARG(int, 0) = -1;
            return;
        }

        /* find a free file descriptor entry on the table */
        int fdesc_idx = -1;
        for(int i = 0; i < FILE_DESC_CNT_MAX; i++) {
            if(running_task->fdtable[i].used == false) {
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

        /* register new file descriptor to the task */
        struct fdtable *fdesc = &running_task->fdtable[fdesc_idx];
        fdesc->file = files[file_idx];
        fdesc->flags = flags;
        fdesc->used = true;

        /* increase file descriptor count of the task */
        running_task->fd_cnt++;

        /* return the file descriptor */
        int fd = fdesc_idx + TASK_CNT_MAX;
        SYSCALL_ARG(int, 0) = fd;
    }
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

    /* check if the file descriptor is indeed marked as used */
    if((running_task->fdtable[fdesc_idx].used != true)) {
        SYSCALL_ARG(long, 0) = EBADF;
        return;
    }

    /* free the file descriptor */
    running_task->fdtable[fdesc_idx].used = false;
    running_task->fd_cnt--;

    /* return on success */
    SYSCALL_ARG(long, 0) = 0;
}

void sys_read(void)
{
    /* read syscall argument */
    int fd = SYSCALL_ARG(int, 0);
    char *buf = SYSCALL_ARG(uint8_t *, 1);
    size_t count = SYSCALL_ARG(size_t, 2);

    /* get the file pointer */
    struct file *filp;
    if(fd < TASK_CNT_MAX) {
        /* anonymous pipe hold by each tasks */
        filp = files[fd];
    } else {
        int fdesc_idx = fd - TASK_CNT_MAX;
        filp = running_task->fdtable[fdesc_idx].file;
        filp->f_flags = running_task->fdtable[fdesc_idx].flags;
    }

    /* read the file */
    ssize_t retval = filp->f_op->read(filp, buf, count, 0);

    /* pass the return value only if the read operation is complete */
    if(running_task->syscall_pending == false) {
        SYSCALL_ARG(int, 0) = retval;
    }
}

void sys_write(void)
{
    /* read syscall arguments */
    int fd = SYSCALL_ARG(int, 0);
    char *buf = SYSCALL_ARG(uint8_t *, 1);
    size_t count = SYSCALL_ARG(size_t, 2);

    /* get the file pointer */
    struct file *filp;
    if(fd < TASK_CNT_MAX) {
        /* anonymous pipe hold by each tasks */
        filp = files[fd];
    } else {
        int fdesc_idx = fd - TASK_CNT_MAX;
        filp = running_task->fdtable[fdesc_idx].file;
        filp->f_flags = running_task->fdtable[fdesc_idx].flags;
    }

    /* write the file */
    ssize_t retval = filp->f_op->write(filp, buf, count, 0);

    /* return the write result */
    SYSCALL_ARG(int, 0) = retval;
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

    /* get the file pointer from the table */
    int fdesc_idx = fd - TASK_CNT_MAX;
    struct file *filp = running_task->fdtable[fdesc_idx].file;

    /* adjust call lseek implementation */
    int retval = filp->f_op->llseek(filp, offset, whence);

    /* return the lseek result */
    SYSCALL_ARG(long, 0) = retval;
}

void sys_fstat(void)
{
    /* read syscall arguments */
    int fd = SYSCALL_ARG(int, 0);
    struct stat *statbuf = SYSCALL_ARG(struct stat *, 1);

    if(fd < TASK_CNT_MAX) {
        SYSCALL_ARG(long, 0) = EBADF;
        return;
    }

    /* read the inode of the given file */
    int fdesc_idx = fd - TASK_CNT_MAX;
    struct inode *inode = running_task->fdtable[fdesc_idx].file->f_inode;

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

    int pid = running_task->pid;

    /* if the pending flag is set that means the request is already sent */
    if(running_task->syscall_pending == false) {
        request_open_directory(pid, pathname);
    }

    /* inquire the file descriptor from the file system task */
    struct inode *inode_dir;
    if(fifo_read(files[pid], (char *)&inode_dir, sizeof(inode_dir), 0) == sizeof(inode_dir)) {
        //return the directory
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
}

void sys_readdir(void)
{
    /* read syscall arguments */
    DIR *dirp = SYSCALL_ARG(DIR *, 0);
    struct dirent *dirent = SYSCALL_ARG(struct dirent *, 1);

    /* pass the return value with r0 */
    SYSCALL_ARG(int, 0) = (uint32_t)fs_read_dir(dirp, dirent);
}

void sys_getpriority(void)
{
    /* return the task priority with r0 */
    SYSCALL_ARG(int, 0) = running_task->priority;
}

void sys_setpriority(void)
{
    /* read syscall arguments */
    int which = SYSCALL_ARG(int, 0);
    int who = SYSCALL_ARG(int, 1);
    int priority = SYSCALL_ARG(int, 2);

    /* unsupported type of the `which' argument */
    if(which != PRIO_PROCESS) {
        /* return on error */
        SYSCALL_ARG(int, 0) = -1;
        return;
    }

    /* compare pid and set new priority */
    int i;
    for(i = 0; i < task_cnt; i++) {
        if(tasks[i].pid == who) {
            tasks[i].priority = priority;

            /* return on success */
            SYSCALL_ARG(int, 0) = 0;
            return;
        }
    }

    /* process not found, return on error */
    SYSCALL_ARG(int, 0) = -1;
}

void sys_getpid(void)
{
    /* return the task pid */
    SYSCALL_ARG(int, 0) = running_task->pid;
}

void sys_mknod(void)
{
    /* read syscall arguments */
    char *pathname = SYSCALL_ARG(char *, 0);
    //mode_t mode = SYSCALL_ARG(mode_t, 1);
    dev_t dev = SYSCALL_ARG(dev_t, 2);

    int pid = running_task->pid;

    /* if the pending flag is set that means the request is already sent */
    if(running_task->syscall_pending == false) {
        request_create_file(pid, pathname, dev);
    }

    /* wait until the file system complete the request */
    int file_idx;
    int retval = fifo_read(files[pid], (char *)&file_idx, sizeof(file_idx), 0);
    if(retval != sizeof(file_idx)) {
        return; //not ready
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

    int pid = running_task->pid;

    /* if the pending flag is set that means the request is already sent */
    if(running_task->syscall_pending == false) {
        request_create_file(pid, pathname, S_IFIFO);
    }

    /* wait until the file system complete the request */
    int file_idx;
    int retval = fifo_read(files[pid], (char *)&pid, sizeof(file_idx), 0);
    if(retval != sizeof(file_idx)) {
        return; //not ready
    }

    if(file_idx == -1) {
        /* file not found, return on error */
        SYSCALL_ARG(int, 0) = -1;
    } else {
        /* return on success */
        SYSCALL_ARG(int, 0) = 0;
    }
}

void sys_mq_open(void)
{
    /* read syscall arguments */
    char *name = SYSCALL_ARG(char *, 0);
    int oflag = SYSCALL_ARG(int, 1);
    struct mq_attr *attr = SYSCALL_ARG(struct mq_attr *, 2);

    if(mq_cnt >= MQUEUE_CNT_MAX) {
        SYSCALL_ARG(mqd_t, 0) = -1;
        return;
    }

    /* initialize the ring buffer */
    struct ringbuf *pipe = ringbuf_create(attr->mq_msgsize, attr->mq_maxmsg);

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
    size_t msg_len = SYSCALL_ARG(size_t, 2);
    unsigned int *msg_prio = SYSCALL_ARG(unsigned int *, 3);

    /* obtain message queue */
    struct msg_queue *mq = &mq_table[mqdes];
    pipe_t *pipe = mq->pipe;

    /* read message */
    ssize_t retval = generic_pipe_read(pipe, msg_ptr, 1);
    if(running_task->syscall_pending == false) {
        SYSCALL_ARG(ssize_t, 0) = retval;
    }
}

void sys_mq_send(void)
{
    /* read syscall arguments */
    mqd_t mqdes = SYSCALL_ARG(mqd_t, 0);
    char *msg_ptr = SYSCALL_ARG(char *, 1);
    size_t msg_len = SYSCALL_ARG(size_t, 2);
    unsigned int msg_prio = SYSCALL_ARG(unsigned int, 3);

    /* obtain message queue */
    struct msg_queue *mq = &mq_table[mqdes];
    pipe_t *pipe = mq->pipe;

    /* send message */
    ssize_t retval = generic_pipe_write(pipe, msg_ptr, 1);
    if(running_task->syscall_pending == false) {
        SYSCALL_ARG(ssize_t, 0) = retval;
    }
}

void sys_pthread_mutex_init(void)
{
    /* read syscall arguments */
    pthread_mutex_t *mutex = SYSCALL_ARG(pthread_mutex_t *, 0);
    pthread_mutex_attr_t *attr = SYSCALL_ARG(pthread_mutex_attr_t *, 1);

    mutex->owner = NULL;
    mutex->lock = 0;
    list_init(&mutex->wait_list);

    SYSCALL_ARG(int, 0) = 0;
}

void sys_pthread_mutex_unlock(void)
{
    /* read syscall arguments */
    pthread_mutex_t *mutex = SYSCALL_ARG(pthread_mutex_t *, 0);

    /* only the owner task can unlock the mutex */
    if(mutex->owner != running_task) {
        /* return on error */
        SYSCALL_ARG(int, 0) = EPERM;
    } else {
        /* release the mutex */
        mutex->owner = NULL;

        /* check the mutex waiting list */
        if(!list_is_empty(&mutex->wait_list)) {
            /* wake up a task from the mutex waiting list */
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
        /* put the current task into the mutex waiting list */
        prepare_to_wait(&mutex->wait_list, &running_task->list, TASK_WAIT);

        /* turn on the syscall pending flag */
        running_task->syscall_pending = true;
    } else {
        /* occupy the mutex by setting the owner */
        mutex->owner = running_task;

        /* turn off the syscall pending flag */
        running_task->syscall_pending = false;

        /* return on success */
        SYSCALL_ARG(int, 0) = 0;
    }
}

void sys_pthread_cond_init(void)
{
    /* read syscall arguments */
    pthread_cond_t *cond = SYSCALL_ARG(pthread_cond_t *, 0);

    list_init(&cond->task_wait_list);

    /* return on success */
    SYSCALL_ARG(int, 0) = 0;
}

void sys_pthread_cond_signal(void)
{
    /* read syscall arguments */
    pthread_cond_t *cond = SYSCALL_ARG(pthread_cond_t*, 0);

    /* wake up a task from the wait list */
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

        /* put the current task into the read waiting list */
        prepare_to_wait(&cond->task_wait_list, &running_task->list, TASK_WAIT);
    }

    /* pthread_cond_wait never returns error code */
    SYSCALL_ARG(int, 0) = 0;
}

void sys_sem_init(void)
{
    /* read syscall arguments */
    sem_t *sem = SYSCALL_ARG(sem_t *, 0);
    int pshared = SYSCALL_ARG(int, 1);
    unsigned int value = SYSCALL_ARG(unsigned int, 2);

    sem->count = value;
    sem->lock = 0;
    list_init(&sem->wait_list);

    /* return on success */
    SYSCALL_ARG(int, 0) = 0;
}

void sys_sem_post(void)
{
    /* read syscall arguments */
    sem_t *sem = SYSCALL_ARG(sem_t *, 0);

    /* prevent integer overflow */
    if(sem->count >= (INT32_MAX - 1)) {
        /* turn on the syscall pending flag */
        running_task->syscall_pending = true;
    } else {
        /* increase the semaphore */
        sem->count++;

        /* wake up a task from the waiting list */
        if(sem->count > 0 && !list_is_empty(&sem->wait_list)) {
            wake_up(&sem->wait_list);
        }

        /* turn off the syscall pending flag */
        running_task->syscall_pending = false;

        /* return on success */
        SYSCALL_ARG(int, 0) = 0;
    }
}

void sys_sem_trywait(void)
{
    /* read syscall arguments */
    sem_t *sem = SYSCALL_ARG(sem_t *, 0);

    if(sem->count <= 0) {
        /* failed to obtain the semaphore, put the current task into the waiting list */
        prepare_to_wait(&sem->wait_list, &running_task->list, TASK_WAIT);

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
        /* failed to obtain the semaphore, put the current task into the waiting list */
        prepare_to_wait(&sem->wait_list, &running_task->list, TASK_WAIT);

        /* turn on the syscall pending flag */
        running_task->syscall_pending = true;
    } else {
        /* successfully obtained the semaphore */
        sem->count--;

        /* turn off the syscall pending flag */
        running_task->syscall_pending = false;

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
                  :: "i"(KERNEL_INT_PRI << 4));
}

void preempt_disable(void)
{
    if(get_proc_mode() == 0) {
        /* privileged mode, disable the irq via syscall */
        set_irq(0);
    } else {
        /* privileged mode, set the basepri register directly */
        set_basepri();
    }
}

void preempt_enable(void)
{
    if(get_proc_mode() == 0) {
        /* unprivileged mode, enable the irq via syscall */
        set_irq(1);
    } else {
        /* privileged mode, set the basepri register directly */
        reset_basepri();
    }
}

void set_syscall_pending(void)
{
    running_task->syscall_pending = true;
}

void reset_syscall_pending(void)
{
    running_task->syscall_pending = false;
}

/* save r0-r3 registers that may carry the syscall arguments via the supervisor call */
static void save_syscall_args(void)
{
    /*
     * the stack layouts are different according to the fpu is used or not due to the lazy context switch mechanism
     * of the arm processor. when lr[4] bit is set as 0, the fpu is used otherwise it is unused.
     */
    if(running_task->stack_top->_lr & 0x10) {
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

void first(void)/* obtain message queue */
{
    /*
     * after the first task finished initiating other tasks,
     * it becomes the idle task and having the lowest priority
     * among all tasks. (i.e., no other tasks should have same
     * or lower priority as the idle task!)
     */
    set_program_name("idle");

    /*=============================*
     * launch the file system task *
     *=============================*/
    if(!fork()) file_system_task();

    /*=======================*
     * mount the file system *
     *=======================*/
    mount("/dev/rom", "/");

    /*================================*
     * launched all hooked user tasks *
     *================================*/
    extern char _tasks_start;
    extern char _tasks_end;

    int func_list_size = ((uint8_t *)&_tasks_end - (uint8_t *)&_tasks_start);
    hook_task_cnt = func_list_size / sizeof(task_func_t);

    /* point to the first task function of the list */
    hook_task_func = (task_func_t *)&_tasks_start;

    int i;
    for(curr_hook_task = 0; curr_hook_task < hook_task_cnt; curr_hook_task++) {
        if(!fork()) {
            (*(hook_task_func + curr_hook_task))();
        }
    }

    while(1); //idle loop when no other task is ready
}

void hook_drivers_init(void)
{
    extern char _drvs_start;
    extern char _drvs_end;

    int func_list_size = ((uint8_t *)&_drvs_end - (uint8_t *)&_drvs_start);
    int drv_cnt = func_list_size / sizeof(drv_init_func_t);

    /* point to the first driver initialization function of the list */
    drv_init_func_t *drv_init_func = (drv_init_func_t *)&_drvs_start;

    int i;
    for(i = 0; i < drv_cnt; i++) {
        (*(drv_init_func + i))();
    }
}

void sched_start(void)
{
    /* set interrupt priorities */
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_4);
    NVIC_SetPriority(SysTick_IRQn, KERNEL_INT_PRI << 4); //set PRI[7:4] = KERNEL_INT_PRI
    NVIC_SetPriority(SVCall_IRQn, KERNEL_INT_PRI << 4);  //set PRI[7:4] = KERNEL_INT_PRI

    uint32_t stack_empty[32]; //a dummy stack for os enviromnent initialization
    os_env_init((uint32_t)(stack_empty + 32) /* point to the top */);

    /* initialize the memory pool */
    memory_pool_init(&mem_pool, mem_pool_buf, MEM_POOL_SIZE);

    /* initialize fifos for all tasks */
    int i;
    for(i = 0; i < TASK_CNT_MAX; i++) {
        fifo_init(i, (struct file **)&files, NULL);
    }

    /* initialize the task ready lists */
    for(i = 0; i <= TASK_MAX_PRIORITY; i++) {
        list_init(&ready_list[i]);
    }

    /* initialize the root file system */
    rootfs_init();

    /* initialized all hooked drivers */
    hook_drivers_init();

    /* initialize the sleep list */
    list_init(&sleep_list);

    task_create(first, 0);

    /* enable the systick timer */
    SysTick_Config(SystemCoreClock / OS_TICK_FREQ);

    /* configure the first task to run */
    running_task = &tasks[0];
    tasks[0].status = TASK_RUNNING;

    while(1) {
        /*
         * if _r7 is negative, the kernel is returned from the systick exception,
         * otherwise it is switched by a syscall request.
         */
        if((int)running_task->stack_top->_r7 < 0) {
            running_task->stack_top->_r7 *= -1; //restore _r7
            tasks_tick_update();
        } else {
            syscall_handler();
        }

        /* select new task */
        schedule();

        /* execute the task if the pending flag of the syscall is not set */
        if(running_task->syscall_pending == false) {
            /* switch to the user space */
            running_task->stack_top = (struct task_stack *)jump_to_user_space((uint32_t)running_task->stack_top);

            /* record the address of syscall arguments in the stack (r0-r3 registers) */
            save_syscall_args();
        }
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
