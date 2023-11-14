#include <errno.h>
#include <fcntl.h>
#include <mqueue.h>
#include <poll.h>
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <task.h>
#include <tenok.h>
#include <time.h>
#include <unistd.h>

#include <arch/port.h>
#include <fs/fs.h>
#include <kernel/bitops.h>
#include <kernel/errno.h>
#include <kernel/interrupt.h>
#include <kernel/kernel.h>
#include <kernel/list.h>
#include <kernel/mqueue.h>
#include <kernel/mutex.h>
#include <kernel/pipe.h>
#include <kernel/printk.h>
#include <kernel/signal.h>
#include <kernel/softirq.h>
#include <kernel/syscall.h>
#include <kernel/task.h>
#include <kernel/thread.h>
#include <kernel/time.h>
#include <kernel/tty.h>
#include <kernel/util.h>
#include <kernel/wait.h>
#include <mm/mm.h>
#include <mm/mpool.h>
#include <mm/page.h>
#include <mm/slab.h>
#include <rom_dev.h>

#include "kconfig.h"

#define PRI_RESERVED 2
#define KTHREAD_PRI_MAX (THREAD_PRIORITY_MAX + PRI_RESERVED)

extern char _user_stack_start;
extern char _user_stack_end;

static LIST_HEAD(tasks_list);   /* list of all tasks in the system */
static LIST_HEAD(threads_list); /* list of all threads in the system */
static LIST_HEAD(sleep_list);   /* list of all threads in the sleeping state */
static LIST_HEAD(suspend_list); /* list of all threads that are suspended */
static LIST_HEAD(timeout_list); /* list of all blocked threads with timeout */
static LIST_HEAD(timers_list);  /* list of all timers in the system */
static LIST_HEAD(poll_list);    /* list of all threads suspended by poll() */
static LIST_HEAD(mqueue_list);  /* list of all posix message queues */

/* lists of all threads in ready state */
struct list_head ready_list[KTHREAD_PRI_MAX + 1];

/* tasks and threads */
static struct task_struct tasks[TASK_CNT_MAX];

static struct thread_info threads[THREAD_CNT_MAX];
static struct thread_info *running_thread = NULL;

static uint32_t bitmap_tasks[BITMAP_SIZE(TASK_CNT_MAX)];
static uint32_t bitmap_threads[BITMAP_SIZE(THREAD_CNT_MAX)];

/* files */
struct file *files[TASK_CNT_MAX + FILE_CNT_MAX];
int file_cnt = 0;

/* file descriptor table */
static struct fdtable fdtable[FILE_DESC_CNT_MAX];
static uint32_t bitmap_fds[BITMAP_SIZE(FILE_DESC_CNT_MAX)];

/* message queue descriptor table */
static struct mq_desc mqd_table[MQUEUE_CNT_MAX];
static uint32_t bitmap_mqds[BITMAP_SIZE(MQUEUE_CNT_MAX)];

/* memory allocators */
static struct kmalloc_slab_info kmalloc_slab_info[] = {
    DEF_KMALLOC_SLAB(32),   DEF_KMALLOC_SLAB(64),  DEF_KMALLOC_SLAB(128),
    DEF_KMALLOC_SLAB(256),  DEF_KMALLOC_SLAB(512), DEF_KMALLOC_SLAB(1024),
#if (PAGE_SIZE_SELECT == PAGE_SIZE_64K)
    DEF_KMALLOC_SLAB(2048),
#endif
};

static struct kmem_cache *kmalloc_caches[KMALLOC_SLAB_TABLE_SIZE];

/* syscall table */
static struct syscall_info syscall_table[] = SYSCALL_TABLE_INIT;

NACKED void thread_return_handler(void)
{
    SYSCALL(THREAD_RETURN_EVENT);
}

NACKED void sig_return_handler(void)
{
    SYSCALL(SIGNAL_CLEANUP_EVENT);
}

NACKED void thread_once_return_handler(void)
{
    SYSCALL(THREAD_ONCE_EVENT);
}

void *kmalloc(size_t size)
{
    void *ptr;

    /* start of the critcal section */
    preempt_disable();

    /* reserve space for kmalloc header */
    const size_t header_size = sizeof(struct kmalloc_header);
    size_t alloc_size = size + header_size;

    /* find a suitable kmalloc slab for memory allocation */
    int i;
    for (i = 0; i < KMALLOC_SLAB_TABLE_SIZE; i++) {
        if (alloc_size <= kmalloc_slab_info[i].size)
            break;
    }

    /* check if the kmalloc slab is found for memory allocation */
    if (i < KMALLOC_SLAB_TABLE_SIZE) {
        /* allocate new memory */
        ptr = kmem_cache_alloc(kmalloc_caches[i], 0);
    } else {
        int page_order = size_to_page_order(size);
        if (page_order != -1) {
            /* allocate directly from the page */
            ptr = alloc_pages(page_order);
        } else {
            /* failed, the reqeust size is too large to handle */
            ptr = NULL;
            printk("kmalloc(): failed as the request size %d is too large",
                   size);
        }
    }

    /* end of the critical section */
    preempt_enable();

    if (ptr) {
        /* record the allocated size and return the start address */
        ((struct kmalloc_header *) ptr)->size = size;
        return (void *) ((uintptr_t) ptr + header_size);
    } else {
        /* return null pointer as error */
        return NULL;
    }
}

void kfree(void *ptr)
{
    /* start of the critical section */
    preempt_disable();

    /* get kmalloc header of the memory */
    const size_t header_size = sizeof(struct kmalloc_header);
    struct kmalloc_header *addr =
        (struct kmalloc_header *) ((uintptr_t) ptr - header_size);

    /* get allocated memory size */
    size_t alloc_size = addr->size;

    /* find the kmalloc slab of the allocate memory */
    int i;
    for (i = 0; i < KMALLOC_SLAB_TABLE_SIZE; i++) {
        if (alloc_size <= kmalloc_slab_info[i].size)
            break;
    }

    if (i < KMALLOC_SLAB_TABLE_SIZE) {
        kmem_cache_free(kmalloc_caches[i], addr);
    } else {
        int page_order = size_to_page_order(alloc_size);
        if (page_order != -1) {
            /* the memory is allocated directly from the page */
            free_pages((unsigned long) addr, page_order);
        } else {
            /* invalid size */
            printk("kfree(): failed as the header is corrupted (address: %p)",
                   addr);
        }
    }

    /* end of the critical section */
    preempt_enable();
}

static inline void set_syscall_pending(struct thread_info *thread)
{
    thread->syscall_pending = true;
}

static inline void reset_syscall_pending(struct thread_info *thread)
{
    thread->syscall_pending = false;
}

inline struct task_struct *current_task_info(void)
{
    return running_thread->task;
}

inline struct thread_info *current_thread_info(void)
{
    return running_thread;
}

static struct task_struct *acquire_task(int pid)
{
    struct task_struct *task;
    list_for_each_entry (task, &tasks_list, list) {
        if (task->pid == pid)
            return task;
    }

    return NULL;
}

struct thread_info *acquire_thread(int tid)
{
    struct thread_info *thread;
    list_for_each_entry (thread, &threads_list, thread_list) {
        if (thread->tid == tid)
            return thread;
    }

    return NULL;
}

/**
 * consume the stack memory from the thread and create an unique
 * anonymous pipe for it
 */
static void *thread_pipe_alloc(uint32_t tid, void *stack_top)
{
    size_t pipe_size = ALIGN(sizeof(struct pipe), sizeof(long));
    size_t kfifo_size = ALIGN(sizeof(struct kfifo), sizeof(long));
    size_t buf_size = ALIGN(sizeof(char) * PIPE_DEPTH, sizeof(long));

    struct pipe *pipe = (struct pipe *) ((uintptr_t) stack_top - pipe_size);
    struct kfifo *pipe_fifo = (struct kfifo *) ((uintptr_t) pipe - kfifo_size);
    char *buf = (char *) ((uintptr_t) pipe_fifo - buf_size);

    kfifo_init(pipe_fifo, buf, sizeof(char), PIPE_DEPTH);
    pipe->fifo = pipe_fifo;
    fifo_init(tid, (struct file **) &files, NULL, pipe);

    return (void *) buf;
}

/**
 * consume the stack memory from the thread and create a signal
 * handler queue
 */
static void *thread_signal_queue_alloc(struct kfifo *signal_queue,
                                       void *stack_top)
{
    size_t payload_size =
        kfifo_header_size() + sizeof(struct staged_handler_info);
    size_t queue_size = ALIGN(payload_size * SIGNAL_QUEUE_SIZE, sizeof(long));
    char *buf = (char *) ((uintptr_t) stack_top - queue_size);
    kfifo_init(signal_queue, buf, payload_size, SIGNAL_QUEUE_SIZE);
    return (void *) buf;
}

static int thread_create(struct thread_info **new_thread,
                         thread_func_t thread_func,
                         struct thread_attr *attr,
                         uint32_t privilege)
{
    /* check if the detach state setting is invalid */
    bool bad_detach_state = attr->detachstate != PTHREAD_CREATE_DETACHED &&
                            attr->detachstate != PTHREAD_CREATE_JOINABLE;

    /* check if the thread priority is invalid */
    bool bad_priority;
    if (privilege == KERNEL_THREAD) {
        bad_priority = attr->schedparam.sched_priority < 0 ||
                       attr->schedparam.sched_priority > KTHREAD_PRI_MAX;
    } else {
        bad_priority = attr->schedparam.sched_priority < 0 ||
                       attr->schedparam.sched_priority > THREAD_PRIORITY_MAX;
    }

    /* check if the scheduling policy is invalid */
    bool bad_sched_policy = attr->schedpolicy != SCHED_RR;

    if (bad_detach_state || bad_priority || bad_sched_policy)
        return -EINVAL;

    /* allocate a new thread id */
    int tid = find_first_zero_bit(bitmap_threads, THREAD_CNT_MAX);
    if (tid >= THREAD_CNT_MAX)
        return -EAGAIN;
    bitmap_set_bit(bitmap_threads, tid);

    /* stack size alignment */
    size_t stack_size = ALIGN(attr->stacksize, sizeof(long));

    /* allocate a new thread */
    struct thread_info *thread = &threads[tid];

    /* reset thread data */
    memset(thread, 0, sizeof(struct thread_info));

    /* allocate stack for the new thread */
    thread->stack = alloc_pages(size_to_page_order(stack_size));
    if (thread->stack == NULL) {
        bitmap_clear_bit(bitmap_threads, tid);
        return -ENOMEM;
    }

    thread->stack_top =
        (struct stack *) ((uintptr_t) thread->stack + stack_size);

    /* allocate anonymous pipe for the new thread */
    thread->stack_top = thread_pipe_alloc(tid, thread->stack_top);

    thread->stack_top =
        thread_signal_queue_alloc(&thread->signal_queue, thread->stack_top);

    /* initialize the thread stack */
    uint32_t args[4] = {0};
    __stack_init((uint32_t **) &thread->stack_top, (uint32_t) thread_func,
                 (uint32_t) thread_return_handler, args);

    /* initialize the thread parameters */
    thread->stack_size = stack_size; /* bytes */
    thread->status = THREAD_WAIT;
    thread->tid = tid;
    thread->priority = attr->schedparam.sched_priority;
    thread->privilege = privilege;

    if (attr->detachstate == PTHREAD_CREATE_DETACHED) {
        thread->joinable = false;
        thread->detached = true;
    } else if (attr->detachstate == PTHREAD_CREATE_JOINABLE) {
        thread->joinable = true;
        thread->detached = false;
    }

    /* initialize the list for poll syscall */
    INIT_LIST_HEAD(&thread->poll_files_list);

    /* initialize the thread join list */
    INIT_LIST_HEAD(&thread->join_list);

    /* record the new thread in the global list */
    list_add(&thread->thread_list, &threads_list);

    /* put the new thread in the sleep list */
    list_add(&thread->list, &sleep_list);

    /* return address to the new thread */
    *new_thread = thread;

    return 0;
}

static int _task_create(thread_func_t task_func,
                        uint8_t priority,
                        int stack_size,
                        uint32_t privilege)
{
    struct thread_attr attr = {
        .schedparam.sched_priority = priority,
        .stackaddr = NULL,
        .stacksize = stack_size,
        .schedpolicy = SCHED_RR,
        .detachstate = PTHREAD_CREATE_JOINABLE,
    };

    struct thread_info *thread;
    int retval = thread_create(&thread, task_func, &attr, privilege);
    if (retval != 0)
        return retval;

    /* allocate a new task id */
    int pid = find_first_zero_bit(bitmap_tasks, TASK_CNT_MAX);
    if (pid >= TASK_CNT_MAX)
        return -1;
    bitmap_set_bit(bitmap_tasks, pid);

    /* allocate a new task */
    struct task_struct *task = &tasks[pid];
    memset(task, 0, sizeof(struct task_struct));
    task->pid = pid;
    task->main_thread = thread;
    INIT_LIST_HEAD(&task->threads_list);
    list_add(&thread->task_list, &task->threads_list);
    list_add(&task->list, &tasks_list);

    /* set the task ownership of the thread */
    thread->task = task;

    return task->pid;
}

int kthread_create(task_func_t task_func, uint8_t priority, int stack_size)
{
    int retval = _task_create(task_func, priority, stack_size, KERNEL_THREAD);
    if (retval < 0)
        printk("kthread_create(): failed to create new task");

    return retval;
}

static void task_delete(struct task_struct *task)
{
    list_del(&task->list);
    bitmap_clear_bit(bitmap_tasks, task->pid);

    for (int i = 0; i < BITMAP_SIZE(FILE_DESC_CNT_MAX); i++) {
        bitmap_fds[i] &= ~task->bitmap_fds[i];
    }

    for (int i = 0; i < BITMAP_SIZE(MQUEUE_CNT_MAX); i++) {
        bitmap_mqds[i] &= ~task->bitmap_mqds[i];
    }
}

static void stage_temporary_handler(struct thread_info *thread,
                                    uint32_t func,
                                    uint32_t return_handler,
                                    uint32_t args[4])
{
    /* preserve the original stack pointer of the thread */
    thread->stack_top_preserved = (uint32_t) thread->stack_top;

    /* prepare a new space on user task's stack for the signal handler */
    __stack_init((uint32_t **) &thread->stack_top, func, return_handler, args);
}

static void stage_signal_handler(struct thread_info *thread,
                                 uint32_t func,
                                 uint32_t args[4])
{
    if (thread->signal_cnt >= SIGNAL_QUEUE_SIZE)
        printk("Warning: the oldest pending signal is overwritten");

    /* push new signal into the pending queue */
    struct staged_handler_info info;
    info.func = func;
    info.args[0] = args[0];
    info.args[1] = args[1];
    info.args[2] = args[2];
    info.args[3] = args[3];
    kfifo_in(&thread->signal_queue, &info, sizeof(struct staged_handler_info));

    /* update the number of pending signals in the queue */
    thread->signal_cnt = kfifo_len(&thread->signal_queue);
}

static void check_pending_signals(void)
{
    if (running_thread->signal_cnt == 0 ||
        running_thread->stack_top_preserved) {
        return;
    }

    /* retrieve pending signal staging request from the queue */
    struct staged_handler_info info;
    kfifo_out(&running_thread->signal_queue, &info,
              sizeof(struct staged_handler_info));

    /* stage the signal handle into the thread stack */
    stage_temporary_handler(running_thread, info.func,
                            (uint32_t) sig_return_handler, info.args);

    /* update the number of pending signals in the queue */
    running_thread->signal_cnt = kfifo_len(&running_thread->signal_queue);
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
    if (thread->status != THREAD_SUSPENDED) {
        return;
    }

    thread->status = THREAD_READY;
    list_move(&thread->list, &ready_list[thread->priority]);
}

static void thread_delete(struct thread_info *thread)
{
    /* remove the thread from the system */
    list_del(&thread->task_list);
    list_del(&thread->thread_list);
    if (thread != running_thread)
        list_del(&thread->list);
    thread->status = THREAD_TERMINATED;
    bitmap_clear_bit(bitmap_threads, thread->tid);

    /* free the stack memory */
    free_pages((uint32_t) thread->stack,
               size_to_page_order(thread->stack_size));

    /* remove the task from the system if it contains no thread anymore */
    struct task_struct *task = current_task_info();
    if (list_empty(&task->threads_list))
        task_delete(task);
}

void prepare_to_wait(struct list_head *wait_list,
                     struct list_head *wait,
                     int state)
{
    list_add(wait, wait_list);

    struct thread_info *thread = list_entry(wait, struct thread_info, list);
    thread->status = state;
}

void wake_up(struct list_head *wait_list)
{
    if (list_empty(wait_list))
        return;

    struct thread_info *highest_pri_thread =
        list_first_entry(wait_list, struct thread_info, list);

    /* find the highest priority task in the waiting list */
    struct thread_info *thread;
    list_for_each_entry (thread, wait_list, list) {
        if (thread->priority > highest_pri_thread->priority)
            highest_pri_thread = thread;
    }

    /* wake up the thread by placing it back to the ready list */
    list_move(&highest_pri_thread->list,
              &ready_list[highest_pri_thread->priority]);
    highest_pri_thread->status = THREAD_READY;
}

void wake_up_all(struct list_head *wait_list)
{
    /* wake up all threads in the waiting list */
    struct list_head *curr, *next;
    list_for_each_safe (curr, next, wait_list) {
        struct thread_info *thread = list_entry(curr, struct thread_info, list);

        list_move(&thread->list, &ready_list[thread->priority]);
        thread->status = THREAD_READY;
    }
}

void finish_wait(struct list_head *wait)
{
    struct thread_info *thread = list_entry(wait, struct thread_info, list);

    if (thread == running_thread)
        return;

    thread->status = THREAD_READY;
    list_move(wait, &ready_list[thread->priority]);
}

static inline void signal_cleanup_handler(void)
{
    running_thread->stack_top =
        (struct stack *) running_thread->stack_top_preserved;
    running_thread->stack_top_preserved = (unsigned long) NULL;
}

static inline void thread_join_handler(void)
{
    /* wake up the threads waiting to join */
    struct list_head *curr, *next;
    list_for_each_safe (curr, next, &running_thread->join_list) {
        struct thread_info *thread = list_entry(curr, struct thread_info, list);

        /* pass the return value back to the waiting thread */
        void **retval = (void **) thread->stack_top->r1;
        if (retval) {
            *(uint32_t *) retval = (uint32_t) running_thread->retval;
        }

        list_move(&thread->list, &ready_list[thread->priority]);
        thread->status = THREAD_READY;
    }

    /* remove the task from the system if it contains no thread anymore */
    struct task_struct *task = current_task_info();
    if (list_empty(&task->threads_list)) {
        /* remove the task from the system */
        task_delete(task);
    }

    /* remove the thread from the system */
    list_del(&running_thread->thread_list);
    list_del(&running_thread->task_list);
    running_thread->status = THREAD_TERMINATED;
    bitmap_clear_bit(bitmap_threads, running_thread->tid);

    /* free the stack memory */
    free_pages((uint32_t) running_thread->stack,
               size_to_page_order(running_thread->stack_size));
}

static struct thread_info *thread_info_find_next(struct thread_info *curr)
{
    struct thread_info *thread = NULL;
    int tid = ((uintptr_t) curr - (uintptr_t) &threads[0]) /
              sizeof(struct thread_info);

    /* find the next thread */
    for (int i = tid + 1; i < THREAD_CNT_MAX; i++) {
        if (bitmap_get_bit(bitmap_threads, i)) {
            thread = &threads[i];
            break;
        }
    }

    return thread;
}

void sys_thread_info(void)
{
    /* read syscall arguments */
    struct thread_stat *info = SYSCALL_ARG(struct thread_stat *, 0);
    void *next = SYSCALL_ARG(void *, 1);

    int tid;
    struct thread_info *thread = NULL;

    if (next == NULL) {
        /* assign the first thread */
        tid = find_first_bit(bitmap_threads, THREAD_CNT_MAX);
        thread = &threads[tid];
    } else {
        /* don't use thread->tid as the thread may be terminated */
        tid = ((uintptr_t) next - (uintptr_t) &threads[0]) /
              sizeof(struct thread_info);

        /* check if the thread is alive */
        if (bitmap_get_bit(bitmap_threads, tid)) {
            /* the thread is still alive */
            thread = (struct thread_info *) next;
        } else {
            /* the thread is not alive, find the next alive one */
            thread = thread_info_find_next(next);

            /* no more alive threads */
            if (!thread) {
                SYSCALL_ARG(void *, 0) = NULL;
                return;
            }
        }
    }

    /* return thread information */
    info->pid = thread->task->pid;
    info->tid = thread->tid;
    info->priority = thread->priority;
    info->status = thread->status;
    info->privileged = thread->privilege == KERNEL_THREAD;
    info->stack_usage =
        (size_t) ((uintptr_t) thread->stack + thread->stack_size -
                  (uintptr_t) thread->stack_top);
    info->stack_size = thread->stack_size;
    strncpy(info->name, thread->name, THREAD_NAME_MAX);

    /* return next thread's address */
    SYSCALL_ARG(void *, 0) = thread_info_find_next(thread);
}

void sys_setprogname(void)
{
    /* read syscall arguments */
    char *name = SYSCALL_ARG(char *, 0);

    strncpy(running_thread->name, name, THREAD_NAME_MAX);
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
    list_add(&(running_thread->list), &sleep_list);

    /* pass the return value with r0 */
    SYSCALL_ARG(int, 0) = 0;
}

void sys_task_create(void)
{
    /* read syscall arguments */
    task_func_t task_func = SYSCALL_ARG(task_func_t, 0);
    int priority = SYSCALL_ARG(int, 1);
    int stack_size = SYSCALL_ARG(int, 2);

    int retval = _task_create(task_func, priority, stack_size, USER_THREAD);
    if (retval < 0)
        printk("task_create(): failed to create new task");

    SYSCALL_ARG(int, 0) = retval;
}

void sys_mpool_alloc(void)
{
    /* read syscall arguments */
    struct mpool *mpool = SYSCALL_ARG(struct mpool *, 0);
    size_t size = SYSCALL_ARG(size_t, 1);

    void *ptr = NULL;
    size_t alloc_size = ALIGN(size, sizeof(long));

    /* test if the memory poll has enough space */
    if ((mpool->offset + alloc_size) <= mpool->size) {
        ptr = (void *) ((uintptr_t) mpool->mem + mpool->offset);
        mpool->offset += alloc_size;
    }

    SYSCALL_ARG(void *, 0) = ptr;
}

void sys_minfo(void)
{
    /* read syscall arguments */
    int name = SYSCALL_ARG(int, 0);

    int retval = -1;

    switch (name) {
    case PAGE_TOTAL_SIZE:
        retval = get_page_total_size();
        break;
    case PAGE_FREE_SIZE:
        retval = get_page_total_free_size();
        break;
    case HEAP_TOTAL_SIZE:
        retval = heap_get_total_size();
        break;
    case HEAP_FREE_SIZE:
        retval = heap_get_free_size();
        break;
    }

    SYSCALL_ARG(int, 0) = retval;
}

void sys_sched_yield(void)
{
    /* suspend the current thread */
    prepare_to_wait(&sleep_list, &running_thread->list, THREAD_WAIT);
}

void sys_exit(void)
{
    /* acquire the task of the running thread */
    struct task_struct *task = current_task_info();

    /* read syscall arguments */
    int status = SYSCALL_ARG(int, 0);

    printk("exit(): task terminated (name: %s, pid: %d, status: %d)",
           task->main_thread->name, task->pid, status);

    /* remove all threads of the task from the system */
    struct list_head *curr, *next;
    list_for_each_safe (curr, next, &task->threads_list) {
        struct thread_info *thread =
            list_entry(curr, struct thread_info, task_list);

        /* remove thread from the system */
        list_del(&thread->thread_list);
        list_del(&thread->task_list);
        list_del(&thread->list);
        thread->status = THREAD_TERMINATED;
        bitmap_clear_bit(bitmap_threads, thread->tid);

        /* free the stack memory */
        free_pages((uint32_t) thread->stack,
                   size_to_page_order(thread->stack_size));
    }

    /* remove the task from the system */
    task_delete(task);
}

void sys_mount(void)
{
    /* read syscall arguments */
    char *source = SYSCALL_ARG(char *, 0);
    char *target = SYSCALL_ARG(char *, 1);

    int tid = running_thread->tid;

    /* send mount request to the file system daemon */
    if (running_thread->syscall_pending == false) {
        request_mount(tid, source, target);
    }

    /* read mount result from the file system daemon */
    int mnt_result;
    int retval =
        fifo_read(files[tid], (char *) &mnt_result, sizeof(mnt_result), 0);

    /* check if the syscall need to be restarted */
    if (retval == -ERESTARTSYS) {
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

    /* acquire the running task */
    struct task_struct *task = current_task_info();

    /* acquire the thread id */
    int tid = running_thread->tid;

    /* send file open request to the file system daemon */
    if (running_thread->syscall_pending == false) {
        request_open_file(tid, pathname);
    }

    /* read the file index from the file system daemon */
    int file_idx;
    int retval = fifo_read(files[tid], (char *) &file_idx, sizeof(file_idx), 0);

    /* check if the syscall need to be restarted */
    if (retval == -ERESTARTSYS) {
        /* file system daemon has not finished the request yet */
        set_syscall_pending(running_thread);
        return;
    } else {
        /* request is complete */
        reset_syscall_pending(running_thread);
    }

    /* file not found */
    if (file_idx == -1) {
        /* return error */
        SYSCALL_ARG(int, 0) = -1;
        return;
    }

    /* find a free file descriptor entry on the table */
    int fdesc_idx = find_first_zero_bit(bitmap_fds, FILE_DESC_CNT_MAX);
    if (fdesc_idx >= FILE_DESC_CNT_MAX) {
        /* return error */
        SYSCALL_ARG(int, 0) = -ENOMEM;
        return;
    }
    bitmap_set_bit(bitmap_fds, fdesc_idx);
    bitmap_set_bit(task->bitmap_fds, fdesc_idx);

    struct file *filp = files[file_idx];

    /* register new file descriptor to the task */
    struct fdtable *fdesc = &fdtable[fdesc_idx];
    fdesc->file = filp;
    fdesc->flags = flags;

    /* check if the file operation is undefined */
    if (!filp->f_op->open) {
        /* release the file descriptor */
        bitmap_clear_bit(bitmap_fds, fdesc_idx);
        bitmap_clear_bit(task->bitmap_fds, fdesc_idx);

        /* return error */
        SYSCALL_ARG(int, 0) = -ENXIO;
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

    /* acquire the running task */
    struct task_struct *task = current_task_info();

    /* a valid file descriptor number should starts from TASK_CNT_MAX */
    if (fd < TASK_CNT_MAX) {
        SYSCALL_ARG(int, 0) = -EBADF;
        return;
    }

    /* calculate the index number of the file descriptor on the table */
    int fdesc_idx = fd - TASK_CNT_MAX;

    /* check if the file descriptor is indeed used */
    if (!bitmap_get_bit(bitmap_fds, fdesc_idx) ||
        !bitmap_get_bit(task->bitmap_fds, fdesc_idx)) {
        SYSCALL_ARG(int, 0) = -EBADF;
        return;
    }

    /* free the file descriptor */
    bitmap_clear_bit(bitmap_fds, fdesc_idx);
    bitmap_clear_bit(task->bitmap_fds, fdesc_idx);

    /* return sucesss */
    SYSCALL_ARG(int, 0) = 0;
}

void sys_dup(void)
{
    /* read syscall arguments */
    int oldfd = SYSCALL_ARG(int, 0);

    if (oldfd < TASK_CNT_MAX) {
        SYSCALL_ARG(int, 0) = -EBADF;
        return;
    }

    /* convert oldfd to the index numbers of the table */
    int old_fdesc_idx = oldfd - TASK_CNT_MAX;

    /* acquire the running task */
    struct task_struct *task = current_task_info();

    /* check if the file descriptor is invalid */
    if (!bitmap_get_bit(bitmap_fds, old_fdesc_idx) ||
        !bitmap_get_bit(task->bitmap_fds, old_fdesc_idx)) {
        SYSCALL_ARG(ssize_t, 0) = -EBADF;
        return;
    }

    /* find a free file descriptor entry on the table */
    int fdesc_idx = find_first_zero_bit(bitmap_fds, FILE_DESC_CNT_MAX);
    if (fdesc_idx >= FILE_DESC_CNT_MAX) {
        /* return error */
        SYSCALL_ARG(int, 0) = -ENOMEM;
        return;
    }
    bitmap_set_bit(bitmap_fds, fdesc_idx);
    bitmap_set_bit(task->bitmap_fds, fdesc_idx);

    /* copy the old file descriptor content to the new one */
    fdtable[fdesc_idx] = fdtable[old_fdesc_idx];

    /* return new file descriptor */
    int fd = fdesc_idx + TASK_CNT_MAX;
    SYSCALL_ARG(int, 0) = fd;
}

void sys_dup2(void)
{
    /* read syscall arguments */
    int oldfd = SYSCALL_ARG(int, 0);
    int newfd = SYSCALL_ARG(int, 1);

    /* convert fds to the index numbers of the table */
    int old_fdesc_idx = oldfd - TASK_CNT_MAX;
    int new_fdesc_idx = newfd - TASK_CNT_MAX;

    if (oldfd < TASK_CNT_MAX || newfd < TASK_CNT_MAX) {
        SYSCALL_ARG(int, 0) = -EBADF;
        return;
    }

    /* acquire the running task */
    struct task_struct *task = current_task_info();

    /* check if the file descriptors are invalid */
    if (!bitmap_get_bit(bitmap_fds, old_fdesc_idx) ||
        !bitmap_get_bit(bitmap_fds, new_fdesc_idx) ||
        !bitmap_get_bit(task->bitmap_fds, old_fdesc_idx) ||
        !bitmap_get_bit(task->bitmap_fds, new_fdesc_idx)) {
        SYSCALL_ARG(ssize_t, 0) = -EBADF;
        return;
    }

    /* copy the old file descriptor content to the new one */
    fdtable[new_fdesc_idx] = fdtable[old_fdesc_idx];

    /* return new file descriptor */
    SYSCALL_ARG(int, 0) = newfd;
}

void sys_read(void)
{
    /* read syscall argument */
    int fd = SYSCALL_ARG(int, 0);
    char *buf = SYSCALL_ARG(char *, 1);
    size_t count = SYSCALL_ARG(size_t, 2);

    /* acquire the running task */
    struct task_struct *task = current_task_info();

    /* get the file pointer */
    struct file *filp;
    if (fd < TASK_CNT_MAX) {
        /* anonymous pipe held by each tasks */
        filp = files[fd];
    } else {
        /* calculate the index number of the file descriptor
         * on the table */
        int fdesc_idx = fd - TASK_CNT_MAX;

        /* check if the file descriptor is invalid */
        if (!bitmap_get_bit(bitmap_fds, fdesc_idx) ||
            !bitmap_get_bit(task->bitmap_fds, fdesc_idx)) {
            SYSCALL_ARG(ssize_t, 0) = -EBADF;
            return;
        }

        filp = fdtable[fdesc_idx].file;
        filp->f_flags = fdtable[fdesc_idx].flags;
    }

    /* check if the file operation is undefined */
    if (!filp->f_op->read) {
        /* return error */
        SYSCALL_ARG(ssize_t, 0) = -ENXIO;
        return;
    }

    /* call read operation */
    ssize_t retval = filp->f_op->read(filp, buf, count, 0);

    /* check if the syscall need to be restarted */
    if (retval == -ERESTARTSYS) {
        /* syscall need to be restarted */
        set_syscall_pending(running_thread);
    } else {
        /* syscall is complete, return the result */
        reset_syscall_pending(running_thread);
        SYSCALL_ARG(ssize_t, 0) = retval;
    }
}

void sys_write(void)
{
    /* read syscall arguments */
    int fd = SYSCALL_ARG(int, 0);
    char *buf = SYSCALL_ARG(char *, 1);
    size_t count = SYSCALL_ARG(size_t, 2);

    /* acquire the running task */
    struct task_struct *task = current_task_info();

    /* get the file pointer */
    struct file *filp;
    if (fd < TASK_CNT_MAX) {
        /* anonymous pipe held by each tasks */
        filp = files[fd];
    } else {
        /* calculate the index number of the file descriptor
         * on the table */
        int fdesc_idx = fd - TASK_CNT_MAX;

        /* check if the file descriptor is invalid */
        if (!bitmap_get_bit(bitmap_fds, fdesc_idx) ||
            !bitmap_get_bit(task->bitmap_fds, fdesc_idx)) {
            SYSCALL_ARG(ssize_t, 0) = -EBADF;
            return;
        }

        filp = fdtable[fdesc_idx].file;
        filp->f_flags = fdtable[fdesc_idx].flags;
    }

    /* check if the file operation is undefined */
    if (!filp->f_op->write) {
        /* return error */
        SYSCALL_ARG(ssize_t, 0) = -ENXIO;
        return;
    }

    /* call write operation */
    ssize_t retval = filp->f_op->write(filp, buf, count, 0);

    /* check if the syscall need to be restarted */
    if (retval == -ERESTARTSYS) {
        /* syscall need to be restarted */
        set_syscall_pending(running_thread);
    } else {
        /* syscall is complete, return the result */
        reset_syscall_pending(running_thread);
        SYSCALL_ARG(ssize_t, 0) = retval;
    }
}

void sys_ioctl(void)
{
    /* read syscall arguments */
    int fd = SYSCALL_ARG(int, 0);
    unsigned int cmd = SYSCALL_ARG(unsigned int, 1);
    unsigned long arg = SYSCALL_ARG(unsigned long, 2);

    /* acquire the running task */
    struct task_struct *task = current_task_info();

    /* get the file pointer */
    struct file *filp;
    if (fd < TASK_CNT_MAX) {
        /* anonymous pipe held by each tasks */
        filp = files[fd];
    } else {
        /* calculate the index number of the file descriptor
         * on the table */
        int fdesc_idx = fd - TASK_CNT_MAX;

        /* check if the file descriptor is invalid */
        if (!bitmap_get_bit(bitmap_fds, fdesc_idx) ||
            !bitmap_get_bit(task->bitmap_fds, fdesc_idx)) {
            SYSCALL_ARG(int, 0) = -EBADF;
            return;
        }

        filp = fdtable[fdesc_idx].file;
    }

    /* check if the file operation is undefined */
    if (!filp->f_op->ioctl) {
        /* return error */
        SYSCALL_ARG(int, 0) = -ENXIO;
        return;
    }

    /* call ioctl operation */
    int retval = filp->f_op->ioctl(filp, cmd, arg);

    /* check if the syscall need to be restarted */
    if (retval == -ERESTARTSYS) {
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

    /* acquire the running task */
    struct task_struct *task = current_task_info();

    /* get the file pointer */
    struct file *filp;
    if (fd < TASK_CNT_MAX) {
        /* anonymous pipe held by each tasks */
        filp = files[fd];
    } else {
        /* calculate the index number of the file descriptor
         * on the table */
        int fdesc_idx = fd - TASK_CNT_MAX;

        /* check if the file descriptor is invalid */
        if (!bitmap_get_bit(bitmap_fds, fdesc_idx) ||
            !bitmap_get_bit(task->bitmap_fds, fdesc_idx)) {
            SYSCALL_ARG(off_t, 0) = -EBADF;
            return;
        }

        filp = fdtable[fdesc_idx].file;
    }

    /* check if the file operation is undefined */
    if (!filp->f_op->lseek) {
        /* return error */
        SYSCALL_ARG(off_t, 0) = -ENXIO;
        return;
    }

    /* call lseek operation */
    int retval = filp->f_op->lseek(filp, offset, whence);

    /* check if the syscall need to be restarted */
    if (retval == -ERESTARTSYS) {
        /* syscall need to be restarted */
        set_syscall_pending(running_thread);
    } else {
        /* syscall is complete, return the result */
        reset_syscall_pending(running_thread);
        SYSCALL_ARG(off_t, 0) = retval;
    }
}

void sys_fstat(void)
{
    /* read syscall arguments */
    int fd = SYSCALL_ARG(int, 0);
    struct stat *statbuf = SYSCALL_ARG(struct stat *, 1);

    /* acquire the running task */
    struct task_struct *task = current_task_info();

    if (fd < TASK_CNT_MAX) {
        SYSCALL_ARG(int, 0) = -EBADF;
        return;
    }

    /* calculate the index number of the file descriptor
     * on the table */
    int fdesc_idx = fd - TASK_CNT_MAX;

    /* check if the file descriptor is invalid */
    if (!bitmap_get_bit(bitmap_fds, fdesc_idx) ||
        !bitmap_get_bit(task->bitmap_fds, fdesc_idx)) {
        SYSCALL_ARG(int, 0) = -EBADF;
        return;
    }

    /* get file inode */
    struct inode *inode = fdtable[fdesc_idx].file->f_inode;

    /* check if the inode exists */
    if (inode != NULL) {
        statbuf->st_mode = inode->i_mode;
        statbuf->st_ino = inode->i_ino;
        statbuf->st_rdev = inode->i_rdev;
        statbuf->st_size = inode->i_size;
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
    if (running_thread->syscall_pending == false) {
        request_open_directory(tid, pathname);
    }

    /* read the directory inode from the file system daemon */
    struct inode *inode_dir;
    int retval =
        fifo_read(files[tid], (char *) &inode_dir, sizeof(inode_dir), 0);

    /* check if the syscall need to be restarted */
    if (retval == -ERESTARTSYS) {
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
    if (dirp->inode_dir == NULL) {
        /* return error */
        SYSCALL_ARG(int, 0) = -1;
    } else {
        /* return sucesss */
        SYSCALL_ARG(int, 0) = 0;
    }
}

void sys_readdir(void)
{
    /* read syscall arguments */
    DIR *dirp = SYSCALL_ARG(DIR *, 0);
    struct dirent *dirent = SYSCALL_ARG(struct dirent *, 1);

    /* pass the return value with r0 */
    SYSCALL_ARG(int, 0) = (uint32_t) fs_read_dir(dirp, dirent);
}

void sys_getpid(void)
{
    /* return the task pid */
    struct task_struct *task = current_task_info();
    SYSCALL_ARG(int, 0) = task->pid;
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
    // mode_t mode = SYSCALL_ARG(mode_t, 1);
    dev_t dev = SYSCALL_ARG(dev_t, 2);

    int tid = running_thread->tid;

    /* send file create request to the file system daemon */
    if (running_thread->syscall_pending == false) {
        request_create_file(tid, pathname, dev);
    }

    /* read the file index from the file system daemon  */
    int file_idx;
    int retval = fifo_read(files[tid], (char *) &file_idx, sizeof(file_idx), 0);

    /* check if the syscall need to be restarted */
    if (retval == -ERESTARTSYS) {
        /* file system daemon has not finished the request yet */
        set_syscall_pending(running_thread);
        return;
    } else {
        /* request is complete */
        reset_syscall_pending(running_thread);
    }

    if (file_idx == -1) {
        /* file not found, return error */
        SYSCALL_ARG(int, 0) = -1;
    } else {
        /* return sucesss */
        SYSCALL_ARG(int, 0) = 0;
    }
}

void sys_mkfifo(void)
{
    /* read syscall arguments */
    char *pathname = SYSCALL_ARG(char *, 0);
    // mode_t mode = SYSCALL_ARG(mode_t, 1);

    int tid = running_thread->tid;

    /* send file create request to the file system daemon */
    if (running_thread->syscall_pending == false) {
        request_create_file(tid, pathname, S_IFIFO);
    }

    /* read the file index from the file system daemon */
    int file_idx = 0;
    int retval = fifo_read(files[tid], (char *) &tid, sizeof(file_idx), 0);

    /* check if the syscall need to be restarted */
    if (retval == -ERESTARTSYS) {
        /* file system has not finished the request yet */
        set_syscall_pending(running_thread);
        return;
    } else {
        /* request is complete */
        reset_syscall_pending(running_thread);
    }

    if (file_idx == -1) {
        /* file not found, return error */
        SYSCALL_ARG(int, 0) = -1;
    } else {
        /* return sucesss */
        SYSCALL_ARG(int, 0) = 0;
    }
}

void poll_notify(struct file *notify_file)
{
    /* iterate through all threads that are suspended by poll() syscall */
    struct list_head *curr_thread_l, *next_thread_l;
    list_for_each_safe (curr_thread_l, next_thread_l, &poll_list) {
        struct thread_info *thread =
            list_entry(curr_thread_l, struct thread_info, list);

        struct file *file;
        list_for_each_entry (file, &thread->poll_files_list, list) {
            if (file == notify_file)
                finish_wait(curr_thread_l);
        }
    }
}

void sys_poll(void)
{
    /* read syscall arguments */
    struct pollfd *fds = SYSCALL_ARG(struct pollfd *, 0);
    nfds_t nfds = SYSCALL_ARG(nfds_t, 1);
    int timeout = SYSCALL_ARG(int, 2);

    if (running_thread->syscall_pending == false) {
        /* setup deadline for poll */
        if (timeout > 0) {
            struct timespec tp;
            get_sys_time(&tp);
            time_add(&tp, 0, timeout * 1000000);
            running_thread->syscall_timeout = tp;
        }

        /* initialization */
        INIT_LIST_HEAD(&running_thread->poll_files_list);
        running_thread->syscall_is_timeout = false;

        /* check file events */
        struct file *filp;
        bool no_event = true;

        for (int i = 0; i < nfds; i++) {
            int fd = fds[i].fd - TASK_CNT_MAX;
            filp = fdtable[fd].file;

            uint32_t events = filp->f_events;
            if (~fds[i].events & events) {
                no_event = false;
            }

            /* return events */
            fds[i].revents |= events;
        }

        if (!no_event) {
            /* return sucesss */
            SYSCALL_ARG(int, 0) = 0;
            return;
        }

        /* no events and no timeout: return immediately */
        if (timeout == 0) {
            /* return error */
            SYSCALL_ARG(int, 0) = -1;
            return;
        }

        /* turn on the syscall pending flag */
        set_syscall_pending(running_thread);

        /* suspend the current thread */
        prepare_to_wait(&poll_list, &running_thread->list, THREAD_WAIT);

        /* add current thread into the timeout monitoring list */
        if (timeout > 0)
            list_add(&running_thread->timeout_list, &timeout_list);

        /* record all files for polling */
        for (int i = 0; i < nfds; i++) {
            int fd = fds[i].fd - TASK_CNT_MAX;
            filp = fdtable[fd].file;
            list_add(&filp->list, &running_thread->poll_files_list);
        }
    } else {
        /* turn off the syscall pending flag */
        reset_syscall_pending(running_thread);

        /* clear list of poll files */
        INIT_LIST_HEAD(&running_thread->poll_files_list);

        /* remove the thread from the poll list */
        if (timeout > 0)
            list_del(&running_thread->timeout_list);

        if (running_thread->syscall_is_timeout) {
            /* return error */
            SYSCALL_ARG(int, 0) = -1;
        } else {
            /* return sucesss */
            SYSCALL_ARG(int, 0) = 0;
        }
    }
}

void sys_mq_getattr(void)
{
    /* read syscall arguments */
    mqd_t mqdes = SYSCALL_ARG(mqd_t, 0);
    struct mq_attr *attr = SYSCALL_ARG(struct mq_attr *, 1);

    /* check if the message queue descriptor is invalid */
    struct task_struct *task = current_task_info();
    if (!bitmap_get_bit(bitmap_mqds, mqdes) ||
        !bitmap_get_bit(task->bitmap_mqds, mqdes)) {
        SYSCALL_ARG(long, 0) = -EBADF;
        return;
    }

    /* return attributes */
    attr->mq_flags = mqd_table[mqdes].attr.mq_flags;
    attr->mq_maxmsg = mqd_table[mqdes].attr.mq_maxmsg;
    attr->mq_msgsize = mqd_table[mqdes].attr.mq_msgsize;
    attr->mq_curmsgs = kfifo_len(mqd_table[mqdes].mq->pipe->fifo);

    /* return sucesss */
    SYSCALL_ARG(int, 0) = 0;
}

void sys_mq_setattr(void)
{
    /* read syscall arguments */
    mqd_t mqdes = SYSCALL_ARG(mqd_t, 0);
    struct mq_attr *newattr = SYSCALL_ARG(struct mq_attr *, 1);
    struct mq_attr *oldattr = SYSCALL_ARG(struct mq_attr *, 2);

    struct task_struct *task = current_task_info();
    if (!bitmap_get_bit(bitmap_mqds, mqdes) ||
        !bitmap_get_bit(task->bitmap_mqds, mqdes)) {
        SYSCALL_ARG(long, 0) = -EBADF;
        return;
    }

    /* acquire message queue attributes from the table */
    struct mq_attr *curr_attr = &mqd_table[mqdes].attr;

    /* only the O_NONBLOCK bit field of the mq_flags can be changed */
    if (newattr->mq_flags & ~O_NONBLOCK ||
        newattr->mq_maxmsg != curr_attr->mq_maxmsg ||
        newattr->mq_msgsize != curr_attr->mq_msgsize) {
        /* return error */
        SYSCALL_ARG(int, 0) = -EINVAL;
        return;
    }

    /* preserve old attributes */
    oldattr->mq_flags = curr_attr->mq_flags;
    oldattr->mq_maxmsg = curr_attr->mq_maxmsg;
    oldattr->mq_msgsize = curr_attr->mq_msgsize;
    oldattr->mq_curmsgs = kfifo_len(mqd_table[mqdes].mq->pipe->fifo);

    /* save new mq_flags attribute */
    curr_attr->mq_flags = (~O_NONBLOCK) & newattr->mq_flags;

    /* return sucesss */
    SYSCALL_ARG(int, 0) = 0;
}

struct mqueue *acquire_mqueue(char *name)
{
    /* find the message queue with the given name */
    struct mqueue *mq;
    list_for_each_entry (mq, &mqueue_list, list) {
        if (!strncmp(name, mq->name, FILE_NAME_LEN_MAX))
            return mq; /* found */
    }

    /* not found */
    return NULL;
}

void sys_mq_open(void)
{
    /* read syscall arguments */
    char *name = SYSCALL_ARG(char *, 0);
    int oflag = SYSCALL_ARG(int, 1);
    struct mq_attr *attr = SYSCALL_ARG(struct mq_attr *, 2);

    /* acquire the running task */
    struct task_struct *task = current_task_info();

    /* search the message with the given name */
    struct mqueue *mq = acquire_mqueue(name);

    /* check if new message queue descriptor can be dispatched */
    int mqdes = find_first_zero_bit(bitmap_mqds, MQUEUE_CNT_MAX);
    if (mqdes >= MQUEUE_CNT_MAX) {
        SYSCALL_ARG(mqd_t, 0) = -ENOMEM;
        return;
    }

    /* attribute is not provided */
    struct mq_attr default_attr;
    if (!attr) {
        /* use default attributes */
        default_attr.mq_maxmsg = 16;
        default_attr.mq_msgsize = 50;
        attr = &default_attr;
    }

    /* message queue with specified name exists */
    if (mq) {
        /* both O_CREAT and O_EXCL flags are set */
        if (oflag & O_CREAT && oflag & O_EXCL) {
            /* return error */
            SYSCALL_ARG(mqd_t, 0) = -EEXIST;
            return;
        }

        /* register new message queue descriptor */
        bitmap_set_bit(bitmap_mqds, mqdes);
        bitmap_set_bit(task->bitmap_mqds, mqdes);
        mqd_table[mqdes].mq = mq;
        mqd_table[mqdes].attr = *attr;
        mqd_table[mqdes].attr.mq_flags = oflag;

        /* return message queue descriptor */
        SYSCALL_ARG(mqd_t, 0) = mqdes;
        return;
    }

    /* create flag is not set */
    if (!(oflag & O_CREAT)) {
        /* return error */
        SYSCALL_ARG(mqd_t, 0) = -ENOENT;
        return;
    }

    /* allocate new message queue */
    struct pipe *pipe = __mq_open(attr);
    struct mqueue *new_mq = kmalloc(sizeof(struct mqueue));

    /* memory allocation failure */
    if (!pipe || !new_mq) {
        /* return error */
        SYSCALL_ARG(mqd_t, 0) = -ENOMEM;
        return;
    }

    /* set up the new message queue */
    new_mq->pipe = pipe;
    strncpy(new_mq->name, name, FILE_NAME_LEN_MAX);
    list_add(&new_mq->list, &mqueue_list);

    /* register new message queue descriptor */
    bitmap_set_bit(bitmap_mqds, mqdes);
    bitmap_set_bit(task->bitmap_mqds, mqdes);
    mqd_table[mqdes].mq = new_mq;
    mqd_table[mqdes].attr = *attr;
    mqd_table[mqdes].attr.mq_flags = oflag;

    /* return message queue descriptor */
    SYSCALL_ARG(mqd_t, 0) = mqdes;
}

void sys_mq_close(void)
{
    /* read syscall arguments */
    mqd_t mqdes = SYSCALL_ARG(mqd_t, 0);

    /* check if the message queue descriptor is invalid */
    struct task_struct *task = current_task_info();
    if (!bitmap_get_bit(bitmap_mqds, mqdes) ||
        !bitmap_get_bit(task->bitmap_mqds, mqdes)) {
        SYSCALL_ARG(long, 0) = -EBADF;
        return;
    }

    /* free the message queue descriptor */
    bitmap_clear_bit(bitmap_mqds, mqdes);
    bitmap_clear_bit(task->bitmap_mqds, mqdes);

    /* return sucesss */
    SYSCALL_ARG(long, 0) = 0;
}

void sys_mq_unlink(void)
{
    char *name = SYSCALL_ARG(char *, 0);

    // TODO: string length check

    /* search the message with the given name */
    struct mqueue *mq = acquire_mqueue(name);

    /* check if the message queue exists */
    if (mq) {
        /* remove message queue from the system */
        list_del(&mq->list);
        kfree(mq);

        /* return sucesss */
        SYSCALL_ARG(int, 0) = 0;
    } else {
        /* return error */
        SYSCALL_ARG(int, 0) = -ENOENT;
    }
}

void sys_mq_receive(void)
{
    /* read syscall arguments */
    mqd_t mqdes = SYSCALL_ARG(mqd_t, 0);
    char *msg_ptr = SYSCALL_ARG(char *, 1);
    size_t msg_len = SYSCALL_ARG(size_t, 2);

    /* check if the message queue descriptor is invalid */
    struct task_struct *task = current_task_info();
    if (!bitmap_get_bit(bitmap_mqds, mqdes) ||
        !bitmap_get_bit(task->bitmap_mqds, mqdes)) {
        SYSCALL_ARG(long, 0) = -EBADF;
        return;
    }

    /* obtain message queue */
    struct mqueue *mq = mqd_table[mqdes].mq;
    struct pipe *pipe = mq->pipe;

    /* check if msg_len exceeds maximum size */
    if (msg_len > mqd_table[mqdes].attr.mq_msgsize) {
        SYSCALL_ARG(int, 0) = -EMSGSIZE;
        return;
    }

    /* read message */
    ssize_t retval =
        __mq_receive(pipe, msg_ptr, msg_len, &mqd_table[mqdes].attr);

    /* check if the syscall need to be restarted */
    if (retval == -ERESTARTSYS) {
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
    size_t msg_len = SYSCALL_ARG(size_t, 2);

    /* check if the message queue descriptor is invalid */
    struct task_struct *task = current_task_info();
    if (!bitmap_get_bit(bitmap_mqds, mqdes) ||
        !bitmap_get_bit(task->bitmap_mqds, mqdes)) {
        SYSCALL_ARG(int, 0) = -EBADF;
        return;
    }

    /* obtain message queue */
    struct mqueue *mq = mqd_table[mqdes].mq;
    struct pipe *pipe = mq->pipe;

    /* check if msg_len exceeds maximum size */
    if (msg_len > mqd_table[mqdes].attr.mq_msgsize) {
        SYSCALL_ARG(int, 0) = -EMSGSIZE;
        return;
    }

    /* send message */
    ssize_t retval = __mq_send(pipe, msg_ptr, msg_len, &mqd_table[mqdes].attr);

    /* check if the syscall need to be restarted */
    if (retval == -ERESTARTSYS) {
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
    pthread_attr_t *_attr = SYSCALL_ARG(pthread_attr_t *, 1);
    start_routine_t start_routine = SYSCALL_ARG(start_routine_t, 2);
    // void *arg = SYSCALL_ARG(void *, 3); //TODO

    struct thread_attr *attr = (struct thread_attr *) _attr;

    /* use defualt attributes if user did not provide */
    struct thread_attr default_attr;
    if (attr == NULL) {
        pthread_attr_init((pthread_attr_t *) &default_attr);
        default_attr.schedparam.sched_priority = running_thread->priority;
        attr = &default_attr;
    }

    /* create new thread */
    struct thread_info *thread;
    int retval = thread_create(&thread, (thread_func_t) start_routine, attr,
                               USER_THREAD);
    if (retval != 0) {
        /* failed to create new thread, return error */
        SYSCALL_ARG(int, 0) = retval;
        return;
    }

    strncpy(thread->name, running_thread->name, THREAD_NAME_MAX);

    /* set the task ownership of the thread */
    thread->task = current_task_info();
    list_add(&thread->task_list, &running_thread->task->threads_list);

    /* return the thread id */
    *pthread = thread->tid;

    /* return sucesss */
    SYSCALL_ARG(int, 0) = 0;
}

void sys_pthread_join(void)
{
    /* read syscall arguments */
    pthread_t tid = SYSCALL_ARG(pthread_t, 0);
    // void **retval = SYSCALL_ARG(void **, 0); //TODO

    struct thread_info *thread = acquire_thread(tid);
    if (!thread) {
        /* return error */
        SYSCALL_ARG(int, 0) = -ESRCH;
        return;
    }

    if (thread->detached || !thread->joinable) {
        /* return error */
        SYSCALL_ARG(int, 0) = -EINVAL;
        return;
    }

    /* check deadlock (threads should not join on each other) */
    struct thread_info *check_thread;
    list_for_each_entry (check_thread, &running_thread->join_list, list) {
        /* check if the thread is waiting to join the running thread */
        if (check_thread == thread) {
            /* deadlock identified, return error */
            SYSCALL_ARG(int, 0) = -EDEADLK;
            return;
        }
    }

    list_add(&running_thread->list, &thread->join_list);
    running_thread->status = THREAD_WAIT;

    /* return sucesss after the thread is waken */
    SYSCALL_ARG(int, 0) = 0;
}

void sys_pthread_cancel(void)
{
    /* read syscall arguments */
    pthread_t tid = SYSCALL_ARG(pthread_t, 0);

    struct thread_info *thread = acquire_thread(tid);
    if (!thread) {
        /* return error */
        SYSCALL_ARG(int, 0) = -ESRCH;
        return;
    }

    /* kthread can only be set by the kernel */
    if (thread->privilege == KERNEL_THREAD) {
        /* return error */
        SYSCALL_ARG(int, 0) = -EPERM;
        return;
    }

    thread_delete(thread);

    /* return sucesss */
    SYSCALL_ARG(int, 0) = 0;
}

void sys_pthread_detach(void)
{
    /* read syscall arguments */
    pthread_t tid = SYSCALL_ARG(pthread_t, 0);

    /* check if the thread exists */
    if (!bitmap_get_bit(bitmap_threads, tid)) {
        /* reurn error */
        SYSCALL_ARG(long, 0) = -ESRCH;
        return;
    }

    threads[tid].detached = true;

    /* return sucesss */
    SYSCALL_ARG(int, 0) = 0;
}

void sys_pthread_setschedparam(void)
{
    /* read syscall arguments */
    pthread_t tid = SYSCALL_ARG(pthread_t, 0);
    // int policy = SYSCALL_ARG(int, 1);
    struct sched_param *param = SYSCALL_ARG(struct sched_param *, 2);

    struct thread_info *thread = acquire_thread(tid);
    if (!thread) {
        /* return error */
        SYSCALL_ARG(int, 0) = -ESRCH;
        return;
    }

    /* kthread can only be set by the kernel */
    if (thread->privilege == KERNEL_THREAD) {
        /* return error */
        SYSCALL_ARG(int, 0) = -EPERM;
        return;
    }

    /* invalid priority parameter */
    if (param->sched_priority < 0 ||
        param->sched_priority > THREAD_PRIORITY_MAX) {
        /* return error */
        SYSCALL_ARG(int, 0) = -EINVAL;
        return;
    }

    /* apply settings */
    if (thread->priority_inherited)
        thread->original_priority = param->sched_priority;
    else
        thread->priority = param->sched_priority;

    /* return sucesss */
    SYSCALL_ARG(int, 0) = 0;
}

void sys_pthread_getschedparam(void)
{
    /* read syscall arguments */
    pthread_t tid = SYSCALL_ARG(pthread_t, 0);
    // int *policy = SYSCALL_ARG(int *, 1);
    struct sched_param *param = SYSCALL_ARG(struct sched_param *, 2);

    struct thread_info *thread = acquire_thread(tid);
    if (!thread) {
        /* return error */
        SYSCALL_ARG(int, 0) = -ESRCH;
        return;
    }

    /* kthread can only be set by the kernel */
    if (thread->privilege == KERNEL_THREAD) {
        /* return error */
        SYSCALL_ARG(int, 0) = -EPERM;
        return;
    }

    /* return settings */
    if (thread->priority_inherited)
        param->sched_priority = thread->original_priority;
    else
        param->sched_priority = thread->priority;

    /* return sucesss */
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
    if (thread->wait_for_signal && (sig2bit(signum) & thread->sig_wait_set)) {
        /* disable the signal waiting flag */
        thread->wait_for_signal = false;

        /* wake up the waiting thread and setup return values */
        finish_wait(&thread->list);
        *thread->ret_sig = signum;
        *(int *) thread->reg.r0 = 0;
    }

    switch (signum) {
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

    if (!stage_handler) {
        return;
    }

    int sig_idx = get_signal_index(signum);
    struct sigaction *act = thread->sig_table[sig_idx];

    /* no signal handler is provided */
    if (act == NULL) {
        return;
    } else if (act->sa_handler == NULL) {
        return;
    }

    /* stage the signal or sigaction handler */
    if (act->sa_flags & SA_SIGINFO) {
        uint32_t args[4] = {0};
        args[0] = (uint32_t) signum;
        args[1] = (uint32_t) NULL /*info */;
        args[2] = (uint32_t) NULL /*context*/;
        stage_signal_handler(thread, (uint32_t) act->sa_sigaction, args);
    } else {
        uint32_t args[4] = {0};
        args[0] = (uint32_t) signum;
        stage_signal_handler(thread, (uint32_t) act->sa_handler, args);
    }
}

void sys_pthread_kill(void)
{
    /* read syscall arguments */
    pthread_t tid = SYSCALL_ARG(pthread_t, 0);
    int sig = SYSCALL_ARG(int, 1);

    struct thread_info *thread = acquire_thread(tid);

    /* failed to find the task with the pid */
    if (!thread) {
        /* return error */
        SYSCALL_ARG(int, 0) = -ESRCH;
        return;
    }

    /* kernel thread does not support posix signals */
    if (thread->privilege == KERNEL_THREAD) {
        /* return error */
        SYSCALL_ARG(int, 0) = -EPERM;
        return;
    }

    /* check if the signal number is defined */
    if (!is_signal_defined(sig)) {
        /* return error */
        SYSCALL_ARG(int, 0) = -EINVAL;
        return;
    }

    handle_signal(thread, sig);

    /* return sucesss */
    SYSCALL_ARG(int, 0) = 0;
}

void sys_pthread_exit(void)
{
    /* read syscall arguments */
    void *retval = SYSCALL_ARG(void *, 0);
    running_thread->retval = retval;

    thread_join_handler();
}

void sys_pthread_mutex_unlock(void)
{
    /* read syscall arguments */
    pthread_mutex_t *_mutex = SYSCALL_ARG(pthread_mutex_t *, 0);

    struct mutex *mutex = (struct mutex *) _mutex;
    int retval = mutex_unlock(mutex);

    /* handle priority inheritance */
    if (mutex->protocol == PTHREAD_PRIO_INHERIT && retval == 0 &&
        running_thread->priority_inherited) {
        /* recover the thread priority */
        running_thread->priority = running_thread->original_priority;
    }

    SYSCALL_ARG(int, 0) = retval;
}

void sys_pthread_mutex_lock(void)
{
    /* read syscall arguments */
    pthread_mutex_t *_mutex = SYSCALL_ARG(pthread_mutex_t *, 0);

    struct mutex *mutex = (struct mutex *) _mutex;
    int retval = mutex_lock(mutex);

    /* check if the syscall need to be restarted */
    if (retval == -ERESTARTSYS) { /* failed to acquire the mutex */
        /* handle priority inheritance */
        if (mutex->protocol == PTHREAD_PRIO_INHERIT &&
            mutex->owner->priority < running_thread->priority) {
            /* raise the priority of the owner thread */
            uint8_t old_priority = mutex->owner->priority;
            uint8_t new_priority = running_thread->priority;

            /* move the owner thread from the ready list with lower priority to
             * the new list with raised priority  */
            struct list_head *curr, *next;
            list_for_each_safe (curr, next, &ready_list[old_priority]) {
                struct thread_info *thread =
                    list_entry(curr, struct thread_info, list);
                if (thread == mutex->owner) {
                    list_move(&thread->list, &ready_list[new_priority]);
                }
            }

            /* set new priority and mark the priority inherited flag */
            mutex->owner->priority = new_priority;
            mutex->owner->priority_inherited = true;
        }

        /* turn on the syscall pending flag */
        set_syscall_pending(running_thread);
    } else { /* succeeded to acquire the mutex */
        /* handle priority inheritance */
        if (mutex->protocol == PTHREAD_PRIO_INHERIT) {
            /* preserve thread priority */
            running_thread->original_priority = running_thread->priority;
        }

        /* turn off the syscall pending flag */
        reset_syscall_pending(running_thread);

        /* return sucesss */
        SYSCALL_ARG(int, 0) = retval;
    }
}

void sys_pthread_mutex_trylock(void)
{
    /* read syscall arguments */
    pthread_mutex_t *mutex = SYSCALL_ARG(pthread_mutex_t *, 0);

    int retval = mutex_lock((struct mutex *) mutex);

    /* check if the syscall need to be restarted */
    if (retval == -ERESTARTSYS) {
        /* failed to lock the mutex */
        SYSCALL_ARG(int, 0) = -EBUSY;
    } else {
        /* return sucesss */
        SYSCALL_ARG(int, 0) = retval;
    }
}

void sys_pthread_cond_signal(void)
{
    /* read syscall arguments */
    pthread_cond_t *cond = SYSCALL_ARG(pthread_cond_t *, 0);

    /* wake up a thread from the wait list */
    wake_up(&((struct cond *) cond)->task_wait_list);

    /* pthread_cond_signal never returns error code */
    SYSCALL_ARG(int, 0) = 0;
}

void sys_pthread_cond_broadcast(void)
{
    /* read syscall arguments */
    pthread_cond_t *cond = SYSCALL_ARG(pthread_cond_t *, 0);

    /* wake up all threads from the wait list */
    wake_up_all(&((struct cond *) cond)->task_wait_list);

    /* pthread_cond_signal never returns error code */
    SYSCALL_ARG(int, 0) = 0;
}

void sys_pthread_cond_wait(void)
{
    /* read syscall arguments */
    pthread_cond_t *cond = SYSCALL_ARG(pthread_cond_t *, 0);
    pthread_mutex_t *mutex = SYSCALL_ARG(pthread_mutex_t *, 1);

    if (((struct mutex *) mutex)->owner) {
        /* release the mutex */
        ((struct mutex *) mutex)->owner = NULL;

        /* put the current thread into the read waiting list */
        prepare_to_wait(&((struct cond *) cond)->task_wait_list,
                        &running_thread->list, THREAD_WAIT);
    }

    /* pthread_cond_wait never returns error code */
    SYSCALL_ARG(int, 0) = 0;
}

static void pthread_once_cleanup_handler(void)
{
    /* restore the stack */
    running_thread->stack_top =
        (struct stack *) running_thread->stack_top_preserved;
    running_thread->stack_top_preserved = (unsigned long) NULL;

    /* wakeup all waiting threads and mark once variable as complete */
    struct thread_once *once_control = running_thread->once_control;
    once_control->finished = true;
    wake_up_all(&once_control->wq);
}

void sys_pthread_once(void)
{
    typedef void (*init_routine_t)(void);

    /* read syscall arguments */
    pthread_once_t *_once_control = SYSCALL_ARG(pthread_once_t *, 0);
    init_routine_t init_routine = SYSCALL_ARG(init_routine_t, 1);

    struct thread_once *once_control = (struct thread_once *) _once_control;

    if (once_control->finished)
        return;

    if (once_control->wq.next == NULL || once_control->wq.prev == NULL) {
        /* first thread to call the pthread_once() */
        init_waitqueue_head(&once_control->wq);
    } else {
        /* pthread_once() is already called */
        prepare_to_wait(&once_control->wq, &running_thread->list, THREAD_WAIT);
        return;
    }

    running_thread->once_control = once_control;
    stage_temporary_handler(running_thread, (uint32_t) init_routine,
                            (uint32_t) thread_once_return_handler, NULL);
}

void sys_sem_post(void)
{
    /* read syscall arguments */
    sem_t *sem = SYSCALL_ARG(sem_t *, 0);

    int retval = up((struct semaphore *) sem);
    SYSCALL_ARG(int, 0) = retval;
}

void sys_sem_trywait(void)
{
    /* read syscall arguments */
    sem_t *sem = SYSCALL_ARG(sem_t *, 0);

    int retval = down_trylock((struct semaphore *) sem);
    SYSCALL_ARG(int, 0) = retval;
}

void sys_sem_wait(void)
{
    /* read syscall arguments */
    sem_t *sem = SYSCALL_ARG(sem_t *, 0);

    int retval = down((struct semaphore *) sem);

    /* check if the syscall need to be restarted */
    if (retval == -ERESTARTSYS) {
        /* turn on the syscall pending flag */
        set_syscall_pending(running_thread);
    } else {
        /* turn off the syscall pending flag */
        reset_syscall_pending(running_thread);

        /* return sucesss */
        SYSCALL_ARG(int, 0) = retval;
    }
}

void sys_sem_getvalue(void)
{
    /* read syscall arguments */
    sem_t *sem = SYSCALL_ARG(sem_t *, 0);
    int *sval = SYSCALL_ARG(int *, 0);

    *sval = ((struct semaphore *) sem)->count;

    /* return sucesss */
    SYSCALL_ARG(int, 0) = 0;
}

void sys_sigaction(void)
{
    /* read syscall arguments */
    int signum = SYSCALL_ARG(int, 0);
    struct sigaction *act = SYSCALL_ARG(struct sigaction *, 1);
    struct sigaction *oldact = SYSCALL_ARG(struct sigaction *, 2);

    /* check if the signal number is defined */
    if (!is_signal_defined(signum)) {
        /* return error */
        SYSCALL_ARG(int, 0) = -EINVAL;
        return;
    }

    /* get the address of the signal action on the table */
    int sig_idx = get_signal_index(signum);
    struct sigaction *sig_entry = running_thread->sig_table[sig_idx];

    /* has the signal action already been registed on the table? */
    if (sig_entry) {
        /* preserve old signal action */
        if (oldact) {
            *oldact = *sig_entry;
        }

        /* replace old signal action */
        *sig_entry = *act;
    } else {
        /* allocate memory for new action */
        struct sigaction *new_act = kmalloc(sizeof(act));

        /* failed to allocate new memory */
        if (new_act == NULL) {
            /* return error */
            SYSCALL_ARG(int, 0) = -ENOMEM;
            return;
        }

        /* register the signal action on the table */
        running_thread->sig_table[sig_idx] = new_act;
        *new_act = *act;
    }

    /* return sucesss */
    SYSCALL_ARG(int, 0) = 0;
}

void sys_sigwait(void)
{
    /* read syscall arguments */
    sigset_t *set = SYSCALL_ARG(sigset_t *, 0);
    int *sig = SYSCALL_ARG(int *, 1);

    sigset_t invalid_mask =
        ~(sig2bit(SIGUSR1) | sig2bit(SIGUSR2) | sig2bit(SIGPOLL) |
          sig2bit(SIGSTOP) | sig2bit(SIGCONT) | sig2bit(SIGKILL));

    /* reject request of waiting on undefined signal */
    if (*set & invalid_mask) {
        /* return error */
        SYSCALL_ARG(int, 0) = -EINVAL;
        return;
    }

    /* record the signal set to wait */
    running_thread->sig_wait_set = *set;
    running_thread->ret_sig = sig;
    running_thread->wait_for_signal = true;

    /* put the thread into the signal waiting list */
    prepare_to_wait(&suspend_list, &running_thread->list, THREAD_WAIT);
}

void sys_kill(void)
{
    /* read syscall arguments */
    pid_t pid = SYSCALL_ARG(pid_t, 0);
    int sig = SYSCALL_ARG(int, 1);

    struct task_struct *task = acquire_task(pid);

    /* failed to find the task with the pid */
    if (!task) {
        /* return error */
        SYSCALL_ARG(int, 0) = -ESRCH;
        return;
    }

    /* check if the signal number is defined */
    if (!is_signal_defined(sig)) {
        /* return error */
        SYSCALL_ARG(int, 0) = -EINVAL;
        return;
    }

    struct list_head *curr, *next;
    list_for_each_safe (curr, next, &task->threads_list) {
        struct thread_info *thread =
            list_entry(curr, struct thread_info, task_list);
        handle_signal(thread, sig);
    }

    /* return sucesss */
    SYSCALL_ARG(int, 0) = 0;
}

void sys_raise(void)
{
    /* read syscall arguments */
    int sig = SYSCALL_ARG(int, 0);

    struct task_struct *task = current_task_info();

    /* failed to find the task with the pid */
    if (!task) {
        /* return error */
        SYSCALL_ARG(int, 0) = -ESRCH;
        return;
    }

    /* check if the signal number is defined */
    if (!is_signal_defined(sig)) {
        /* return error */
        SYSCALL_ARG(int, 0) = -EINVAL;
        return;
    }

    struct list_head *curr, *next;
    list_for_each_safe (curr, next, &task->threads_list) {
        struct thread_info *thread =
            list_entry(curr, struct thread_info, task_list);
        handle_signal(thread, sig);
    }

    /* return sucesss */
    SYSCALL_ARG(int, 0) = 0;
}

void sys_clock_gettime(void)
{
    /* read syscall arguments */
    clockid_t clockid = SYSCALL_ARG(clockid_t, 0);
    struct timespec *tp = SYSCALL_ARG(struct timespec *, 1);

    if (clockid != CLOCK_MONOTONIC) {
        /* return error */
        SYSCALL_ARG(int, 0) = -EINVAL;
        return;
    }

    get_sys_time(tp);

    /* return sucesss */
    SYSCALL_ARG(int, 0) = 0;
}

void sys_clock_settime(void)
{
    /* read syscall arguments */
    clockid_t clockid = SYSCALL_ARG(clockid_t, 0);
    struct timespec *tp = SYSCALL_ARG(struct timespec *, 1);

    if (clockid != CLOCK_REALTIME) {
        /* return error */
        SYSCALL_ARG(int, 0) = -EINVAL;
        return;
    }

    set_sys_time(tp);

    /* return sucesss */
    SYSCALL_ARG(int, 0) = 0;
}

static struct timer *acquire_timer(int timerid)
{
    /* find the timer with the id */
    struct timer *timer;
    list_for_each_entry (timer, &running_thread->timers_list, list) {
        /* check timer id */
        if (timerid == timer->id)
            return timer;
    }

    return NULL;
}

void sys_timer_create(void)
{
    /* read syscall arguments */
    clockid_t clockid = SYSCALL_ARG(clockid_t, 0);
    struct sigevent *sevp = SYSCALL_ARG(struct sigevent *, 1);
    timer_t *timerid = SYSCALL_ARG(timer_t *, 2);

    /* unsupported clock source */
    if (clockid != CLOCK_MONOTONIC) {
        /* return error */
        SYSCALL_ARG(int, 0) = -EINVAL;
        return;
    }

    if (sevp->sigev_notify != SIGEV_NONE &&
        sevp->sigev_notify != SIGEV_SIGNAL) {
        /* return error */
        SYSCALL_ARG(int, 0) = -EINVAL;
        return;
    }

    /* allocate memory for the new timer */
    struct timer *new_tm = kmalloc(sizeof(struct timer));

    /* failed to allocate memory */
    if (new_tm == NULL) {
        /* return error */
        SYSCALL_ARG(int, 0) = -ENOMEM;
        return;
    }

    /* record timer settings */
    new_tm->id = running_thread->timer_cnt;
    new_tm->sev = *sevp;
    new_tm->thread = running_thread;

    /* timer list initialization */
    if (running_thread->timer_cnt == 0) {
        /* initialize the timer list head */
        INIT_LIST_HEAD(&running_thread->timers_list);
    }

    /* put the new timer into the list */
    list_add(&new_tm->g_list, &timers_list);
    list_add(&new_tm->list, &running_thread->timers_list);

    /* return timer id */
    *timerid = running_thread->timer_cnt;

    /* increase timer count */
    running_thread->timer_cnt++;

    /* return sucesss */
    SYSCALL_ARG(int, 0) = 0;
}

void sys_timer_delete(void)
{
    /* read syscall arguments */
    timer_t timerid = SYSCALL_ARG(timer_t, 0);

    /* aquire the timer with given id */
    struct timer *timer = acquire_timer(timerid);

    /* failed to acquire the timer */
    if (timer == NULL) {
        /* invalid timer id, return error */
        SYSCALL_ARG(int, 0) = -EINVAL;
        return;
    }

    /* remove the timer from the lists and
     * free the memory */
    list_del(&timer->g_list);
    list_del(&timer->list);
    kfree(timer);

    /* return sucesss */
    SYSCALL_ARG(int, 0) = 0;
}

void sys_timer_settime(void)
{
    /* read syscall arguments */
    timer_t timerid = SYSCALL_ARG(timer_t, 0);
    int flags = SYSCALL_ARG(int, 1);
    struct itimerspec *new_value = SYSCALL_ARG(struct itimerspec *, 2);
    struct itimerspec *old_value = SYSCALL_ARG(struct itimerspec *, 3);

    /* bad arguments */
    if (new_value->it_value.tv_sec < 0 || new_value->it_value.tv_nsec < 0 ||
        new_value->it_value.tv_nsec > 999999999) {
        /* return error */
        SYSCALL_ARG(int, 0) = -EINVAL;
        return;
    }

    /* the task has no timer */
    if (running_thread->timer_cnt == 0) {
        /* return error */
        SYSCALL_ARG(int, 0) = -EINVAL;
        return;
    }

    /* acquire the timer with given id */
    struct timer *timer = acquire_timer(timerid);

    /* failed to acquire the timer */
    if (timer == NULL) {
        /* return error */
        SYSCALL_ARG(int, 0) = -EINVAL;
        return;
    }

    /* return old timer setting */
    if (old_value != NULL)
        *old_value = timer->setting;

    /* save timer's setting */
    timer->flags = flags;
    timer->setting = *new_value;
    timer->ret_time = timer->setting;

    /* enable the timer */
    timer->counter = timer->setting.it_value;
    timer->enabled = true;

    /* return sucesss */
    SYSCALL_ARG(int, 0) = 0;
}

void sys_timer_gettime(void)
{
    /* read syscall arguments */
    timer_t timerid = SYSCALL_ARG(timer_t, 0);
    struct itimerspec *curr_value = SYSCALL_ARG(struct itimerspec *, 1);

    /* acquire the timer with given id */
    struct timer *timer = acquire_timer(timerid);

    /* failed to acquire the timer */
    if (timer == NULL) {
        /* invalid timer id, return error */
        SYSCALL_ARG(int, 0) = -EINVAL;
        return;
    }

    /* return sucesss */
    *curr_value = timer->ret_time;
    SYSCALL_ARG(int, 0) = 0;
}

void sys_malloc(void)
{
    /* read syscall arguments */
    size_t size = SYSCALL_ARG(size_t, 0);

    void *ptr;

    /* return NULL if the size is zerp */
    if (size == 0) {
        SYSCALL_ARG(void *, 0) = NULL;
        return;
    }

    ptr = __malloc(size);

    SYSCALL_ARG(void *, 0) = ptr;
}

void sys_free(void)
{
    /* read syscall arguments */
    __attribute__((unused)) void *ptr = SYSCALL_ARG(void *, 0);

    __free(ptr);
}

static void threads_ticks_update(void)
{
    /* update the sleep timers */
    struct thread_info *thread;
    list_for_each_entry (thread, &sleep_list, list) {
        /* update remained ticks of waiting */
        if (thread->sleep_ticks > 0)
            thread->sleep_ticks--;
    }
}

static void timers_update(void)
{
    struct timer *timer;
    list_for_each_entry (timer, &timers_list, g_list) {
        if (!timer->enabled)
            continue;

        /* update the timer */
        timer_down_count(&timer->counter);

        /* update the return time */
        timer->ret_time.it_value = timer->counter;

        /* check if the time is up */
        if (timer->counter.tv_sec != 0 || timer->counter.tv_nsec != 0) {
            continue; /* no */
        }

        /* reload the timer */
        timer->counter = timer->setting.it_interval;

        /* shutdown the one-shot type timer */
        if (timer->setting.it_interval.tv_sec == 0 &&
            timer->setting.it_interval.tv_nsec == 0) {
            timer->enabled = false;
        }

        /* stage the signal handler */
        if (timer->sev.sigev_notify == SIGEV_SIGNAL) {
            uint32_t args[4] = {0};
            sa_handler_t notify_func =
                (sa_handler_t) timer->sev.sigev_notify_function;
            stage_signal_handler(timer->thread, (uint32_t) notify_func, args);
        }
    }
}

static void syscall_timeout_update(void)
{
    /* get current time */
    struct timespec tp;
    get_sys_time(&tp);

    /* iterate through all tasks that waiting for poll events */
    struct thread_info *thread;
    list_for_each_entry (thread, &timeout_list, timeout_list) {
        /* wake up the thread if the time is up */
        if (tp.tv_sec >= thread->syscall_timeout.tv_sec &&
            tp.tv_nsec >= thread->syscall_timeout.tv_nsec) {
            thread->syscall_is_timeout = true;
            finish_wait(&thread->list);
        }
    }
}

static void system_ticks_update(void)
{
    system_timer_update();
    threads_ticks_update();
    timers_update();
    syscall_timeout_update();
}

static inline bool check_systick_event(void)
{
    /* if r7 is negative, the kernel is returned from the systick
     * interrupt. otherwise the kernel is returned from the
     * supervisor call.
     */
    if ((int) SUPERVISOR_EVENT(running_thread) < 0) {
        SUPERVISOR_EVENT(running_thread) *= -1;  // restore to positive
        return true;
    }

    return false;
}

static inline void prepare_syscall_args(void)
{
    /* the stack layouts are different according to the fpu is used or not due
     * to the lazy context switch mechanism of the arm processor. when lr[4] bit
     * is set as 0, the fpu is used otherwise it is unused.
     */
    if (running_thread->stack_top->_lr & 0x10) {
        struct stack *sp = (struct stack *) running_thread->stack_top;
        running_thread->reg.r0 = &sp->r0;
        running_thread->reg.r1 = &sp->r1;
        running_thread->reg.r2 = &sp->r2;
        running_thread->reg.r3 = &sp->r3;
    } else {
        struct stack_fpu *sp_fpu =
            (struct stack_fpu *) running_thread->stack_top;
        running_thread->reg.r0 = &sp_fpu->r0;
        running_thread->reg.r1 = &sp_fpu->r1;
        running_thread->reg.r2 = &sp_fpu->r2;
        running_thread->reg.r3 = &sp_fpu->r3;
    }
}

static void syscall_handler(void)
{
    for (int i = 0; i < SYSCALL_CNT; i++) {
        if (SUPERVISOR_EVENT(running_thread) == syscall_table[i].num) {
            /* execute the syscall service */
            prepare_syscall_args();
            syscall_table[i].syscall_handler();
            break;
        }
    }
}

static void thread_return_event_handler(void)
{
    struct task_struct *task = current_task_info();
    if (running_thread == task->main_thread) {
        /* returned from the main thread, the whole task should
         * be terminated */
        sys_exit();
    } else {
        running_thread->retval = (void *) running_thread->stack_top->r0;
        thread_join_handler();
    }
}

static void supervisor_request_handler(void)
{
    if (SUPERVISOR_EVENT(running_thread) == SIGNAL_CLEANUP_EVENT) {
        signal_cleanup_handler();
    } else if (SUPERVISOR_EVENT(running_thread) == THREAD_RETURN_EVENT) {
        thread_return_event_handler();
    } else if (SUPERVISOR_EVENT(running_thread) == THREAD_ONCE_EVENT) {
        pthread_once_cleanup_handler();
    } else {
        syscall_handler();
    }
}

static void schedule(void)
{
    /* stop current thread */
    if (running_thread->status == THREAD_RUNNING)
        prepare_to_wait(&sleep_list, &running_thread->list, THREAD_WAIT);

    /* awaken sleep threads if the sleep tick exhausted */
    struct list_head *curr, *next;
    list_for_each_safe (curr, next, &sleep_list) {
        /* obtain the thread info */
        struct thread_info *thread = list_entry(curr, struct thread_info, list);

        /* move the thread into the ready list if it is ready */
        if (thread->sleep_ticks == 0) {
            list_move(curr, &ready_list[thread->priority]);
            thread->status = THREAD_READY;
        }
    }

    /* find a ready list that contains runnable threads */
    int pri;
    for (pri = KTHREAD_PRI_MAX; pri >= 0; pri--) {
        if (list_empty(&ready_list[pri]) == false)
            break;
    }

    /* select a task from the ready list */
    running_thread =
        list_first_entry(&ready_list[pri], struct thread_info, list);
    running_thread->status = THREAD_RUNNING;
    list_del(&running_thread->list);
}

static void print_platform_info(void)
{
    printk("Tenok RTOS (built time: %s %s)", __TIME__, __DATE__);
    printk("Machine model: %s", __BOARD_NAME__);
}

static void slab_init(void)
{
    kmem_cache_init();

    /* initialize slabs for kmalloc */
    for (int i = 0; i < KMALLOC_SLAB_TABLE_SIZE; i++) {
        kmalloc_caches[i] = kmem_cache_create(kmalloc_slab_info[i].name,
                                              kmalloc_slab_info[i].size,
                                              sizeof(uint32_t), 0, NULL);
    }
}

void init(void)
{
    setprogname("idle");

    /* platform initialization */
    preempt_disable();
    {
        print_platform_info();
        __platform_init();
        rom_dev_init();
    }
    preempt_enable();

    /* mount rom file system */
    mount("/dev/rom", "/");

    /* launched all hooked user tasks */
    extern char _tasks_start;
    extern char _tasks_end;

    int func_list_size = ((uintptr_t) &_tasks_end - (uintptr_t) &_tasks_start);
    int hook_task_cnt = func_list_size / sizeof(struct task_hook);

    struct task_hook *hook_task = (struct task_hook *) &_tasks_start;

    for (int i = 0; i < hook_task_cnt; i++) {
        task_create(hook_task[i].task_func, hook_task[i].priority,
                    hook_task[i].stacksize);
    }

    /* idle loop with lowest thread priority */
    while (1) {
        __idle();
    }
}

void sched_start(void)
{
    /* initialize the irq vector table */
    irq_init();

    uint32_t stack_empty[32];  // a dummy stack for os enviromnent
                               // initialization
    os_env_init(&stack_empty[31]);

    slab_init();
    heap_init();
    tty_init();
    rootfs_init();

    /* initialize the ready lists */
    for (int i = 0; i <= KTHREAD_PRI_MAX; i++) {
        INIT_LIST_HEAD(&ready_list[i]);
    }


    /* initialize kernel threads and deamons */
    kthread_create(init, 0, 1024);
    kthread_create(softirqd, KTHREAD_PRI_MAX, 1024);
    kthread_create(filesysd, KTHREAD_PRI_MAX - 1, 1024);

    /* manually set task 0 as the first thread to run */
    running_thread = &threads[0];
    threads[0].status = THREAD_RUNNING;
    list_del(&threads[0].list);  // remove from the sleep list

    while (1) {
        if (check_systick_event()) {
            system_ticks_update();
        } else {
            supervisor_request_handler();
        }

        /* select new thread */
        schedule();

        /* jump to the syscall handler as a thread with pending
         * syscall is selected */
        if (running_thread->syscall_pending)
            continue;

        /* check if the selected thread has pending signals */
        check_pending_signals();

        /* jump to the selected thread */
        running_thread->stack_top = jump_to_thread(running_thread->stack_top,
                                                   running_thread->privilege);
    }
}
