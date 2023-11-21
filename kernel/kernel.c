#include <errno.h>
#include <fcntl.h>
#include <mpool.h>
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
#include <common/bitops.h>
#include <common/list.h>
#include <common/util.h>
#include <fs/fs.h>
#include <kernel/errno.h>
#include <kernel/interrupt.h>
#include <kernel/kernel.h>
#include <kernel/mqueue.h>
#include <kernel/mutex.h>
#include <kernel/pipe.h>
#include <kernel/printk.h>
#include <kernel/semaphore.h>
#include <kernel/signal.h>
#include <kernel/softirq.h>
#include <kernel/syscall.h>
#include <kernel/thread.h>
#include <kernel/time.h>
#include <kernel/tty.h>
#include <kernel/wait.h>
#include <mm/mm.h>
#include <mm/page.h>
#include <mm/slab.h>
#include <rom_dev.h>

#include "kconfig.h"

#define PRI_RESERVED 2
#define KTHREAD_PRI_MAX (THREAD_PRIORITY_MAX + PRI_RESERVED)

static LIST_HEAD(tasks_list);   /* List of all tasks in the system */
static LIST_HEAD(threads_list); /* List of all threads in the system */
static LIST_HEAD(sleep_list);   /* List of all threads in the sleeping state */
static LIST_HEAD(suspend_list); /* List of all threads that are suspended */
static LIST_HEAD(timeout_list); /* List of all blocked threads with timeout */
static LIST_HEAD(timers_list);  /* List of all timers in the system */
static LIST_HEAD(poll_list);    /* List of all threads suspended by poll() */
static LIST_HEAD(mqueue_list);  /* List of all posix message queues */

/* Lists of all threads in ready state */
struct list_head ready_list[KTHREAD_PRI_MAX + 1];

/* Tasks and threads */
static struct task_struct tasks[TASK_CNT_MAX];

static struct thread_info threads[THREAD_CNT_MAX];
static struct thread_info *running_thread = NULL;

static uint32_t bitmap_tasks[BITMAP_SIZE(TASK_CNT_MAX)];
static uint32_t bitmap_threads[BITMAP_SIZE(THREAD_CNT_MAX)];

/* Daemons information */
static int daemon_id_table[DAEMON_CNT];

#define DECLARE_DAEMON(x) #x
static char *deamon_names[] = {DAEMON_LIST};
#undef DECLARE_DAEMON

/* Files */
struct file *files[TASK_CNT_MAX + FILE_CNT_MAX];
int file_cnt = 0;

/* File descriptor table */
static struct fdtable fdtable[FILE_DESC_CNT_MAX];
static uint32_t bitmap_fds[BITMAP_SIZE(FILE_DESC_CNT_MAX)];

/* Message queue descriptor table */
static struct mq_desc mqd_table[MQUEUE_CNT_MAX];
static uint32_t bitmap_mqds[BITMAP_SIZE(MQUEUE_CNT_MAX)];

/* Memory allocators */
static struct kmalloc_slab_info kmalloc_slab_info[] = {
    /* clang-format off */
    DEF_KMALLOC_SLAB(32),
    DEF_KMALLOC_SLAB(64),
    DEF_KMALLOC_SLAB(128),
    DEF_KMALLOC_SLAB(256),
    DEF_KMALLOC_SLAB(512),
    DEF_KMALLOC_SLAB(1024),
#if (PAGE_SIZE_SELECT == PAGE_SIZE_64K)
    DEF_KMALLOC_SLAB(2048),
#endif
    /* clang-format on */
};

static struct kmem_cache *kmalloc_caches[KMALLOC_SLAB_TABLE_SIZE];

/* Syscall table */
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

    /* Start the critcal section */
    preempt_disable();

    /* Reserve space for kmalloc header */
    const size_t header_size = sizeof(struct kmalloc_header);
    size_t alloc_size = size + header_size;

    /* Find a suitable kmalloc slab */
    int i;
    for (i = 0; i < KMALLOC_SLAB_TABLE_SIZE; i++) {
        if (alloc_size <= kmalloc_slab_info[i].size)
            break;
    }

    /* Check if a kmalloc slab with suitable size is found */
    if (i < KMALLOC_SLAB_TABLE_SIZE) {
        /* Allocate new memory */
        ptr = kmem_cache_alloc(kmalloc_caches[i], 0);
    } else {
        int page_order = size_to_page_order(size);
        if (page_order != -1) {
            /* Allocate the memory directly from the page */
            ptr = alloc_pages(page_order);
        } else {
            /* Failed, the reqeust size is too large to handle */
            ptr = NULL;
            printk("kmalloc(): failed as the request size %d is too large",
                   size);
        }
    }

    /* End the critical section */
    preempt_enable();

    if (ptr) {
        /* Record the allocated size and return the start address */
        ((struct kmalloc_header *) ptr)->size = size;
        return (void *) ((uintptr_t) ptr + header_size);
    } else {
        /* Return null pointer as error */
        return NULL;
    }
}

void kfree(void *ptr)
{
    /* Start the critical section */
    preempt_disable();

    /* Get kmalloc header */
    const size_t header_size = sizeof(struct kmalloc_header);
    struct kmalloc_header *addr =
        (struct kmalloc_header *) ((uintptr_t) ptr - header_size);

    /* Get allocated size */
    size_t alloc_size = addr->size;

    /* Find the kmalloc slab that the memory belongs to */
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
            /* The memory is allocated directly from the page */
            free_pages((unsigned long) addr, page_order);
        } else {
            /* Invalid size */
            printk("kfree(): failed as the header is corrupted (address: %p)",
                   addr);
        }
    }

    /* End the critical section */
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

void set_daemon_id(int daemon)
{
    preempt_disable();

    /* Calling thread is not the daemon itself */
    if (strcasecmp(deamon_names[daemon], running_thread->name)) {
        while (1)
            ;
    }

    daemon_id_table[daemon] = running_thread->tid;

    preempt_enable();
}

uint16_t get_daemon_id(int daemon)
{
    return daemon_id_table[daemon];
}

/* Consume the stack memory from the thread and create an unique
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

/* Consume the stack memory from the thread and create a signal
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
                         void *thread_arg,
                         uint32_t privilege)
{
    /* Check if the detach state setting is invalid */
    bool bad_detach_state = attr->detachstate != PTHREAD_CREATE_DETACHED &&
                            attr->detachstate != PTHREAD_CREATE_JOINABLE;

    /* Check if the thread priority is invalid */
    bool bad_priority;
    if (privilege == KERNEL_THREAD) {
        bad_priority = attr->schedparam.sched_priority < 0 ||
                       attr->schedparam.sched_priority > KTHREAD_PRI_MAX;
    } else {
        bad_priority = attr->schedparam.sched_priority < 0 ||
                       attr->schedparam.sched_priority > THREAD_PRIORITY_MAX;
    }

    /* Check if the scheduling policy is invalid */
    bool bad_sched_policy = attr->schedpolicy != SCHED_RR;

    if (bad_detach_state || bad_priority || bad_sched_policy)
        return -EINVAL;

    /* Allocate new thread Id */
    int tid = find_first_zero_bit(bitmap_threads, THREAD_CNT_MAX);
    if (tid >= THREAD_CNT_MAX)
        return -EAGAIN;
    bitmap_set_bit(bitmap_threads, tid);

    /* Force the stack size to be aligned */
    size_t stack_size = ALIGN(attr->stacksize, sizeof(long));

    /* Allocate new thread control block */
    struct thread_info *thread = &threads[tid];

    /* Reset thread data */
    memset(thread, 0, sizeof(struct thread_info));

    /* Allocate thread stack memory */
    thread->stack = alloc_pages(size_to_page_order(stack_size));
    if (thread->stack == NULL) {
        bitmap_clear_bit(bitmap_threads, tid);
        return -ENOMEM;
    }

    thread->stack_top =
        (unsigned long *) ((uintptr_t) thread->stack + stack_size);

    /* Allocate anonymous pipe for the thread */
    thread->stack_top = thread_pipe_alloc(tid, thread->stack_top);

    thread->stack_top =
        thread_signal_queue_alloc(&thread->signal_queue, thread->stack_top);

    /* Initialize thread stack */
    uint32_t func_args[4] = {0};
    if (thread_arg)
        func_args[0] = (uint32_t) thread_arg;
    __stack_init((uint32_t **) &thread->stack_top, (uint32_t) thread_func,
                 (uint32_t) thread_return_handler, func_args);

    /* Initialize thread parameters */
    thread->stack_size = stack_size; /* Bytes */
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

    /* Initialize poll file list */
    INIT_LIST_HEAD(&thread->poll_files_list);

    /* Initialize the thread join list */
    INIT_LIST_HEAD(&thread->join_list);

    /* Link the thread to the global thread list */
    list_add(&thread->thread_list, &threads_list);

    /* Enqueue the thread into the sleep list */
    list_add(&thread->list, &sleep_list);

    /* Return the pointer of the thread */
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
    int retval = thread_create(&thread, task_func, &attr, NULL, privilege);
    if (retval != 0)
        return retval;

    /* Allocate new task ID */
    int pid = find_first_zero_bit(bitmap_tasks, TASK_CNT_MAX);
    if (pid >= TASK_CNT_MAX)
        return -1;
    bitmap_set_bit(bitmap_tasks, pid);

    /* Allocate new task control block */
    struct task_struct *task = &tasks[pid];
    memset(task, 0, sizeof(struct task_struct));
    task->pid = pid;
    task->main_thread = thread;
    INIT_LIST_HEAD(&task->threads_list);
    list_add(&thread->task_list, &task->threads_list);
    list_add(&task->list, &tasks_list);

    /* Set the task ownership to the thread */
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
    /* Preserve original stack pointer of the thread */
    thread->stack_top_preserved = (uint32_t) thread->stack_top;

    /* Stage new stack for executing handler function on the top of the old
     * stack */
    __stack_init((uint32_t **) &thread->stack_top, func, return_handler, args);
}

static void stage_signal_handler(struct thread_info *thread,
                                 uint32_t func,
                                 uint32_t args[4])
{
    if (thread->signal_cnt >= SIGNAL_QUEUE_SIZE)
        printk("Warning: the oldest pending signal is overwritten");

    /* Push new signal into the pending queue */
    struct staged_handler_info info;
    info.func = func;
    info.args[0] = args[0];
    info.args[1] = args[1];
    info.args[2] = args[2];
    info.args[3] = args[3];
    kfifo_in(&thread->signal_queue, &info, sizeof(struct staged_handler_info));

    /* Update the number of total pending signals */
    thread->signal_cnt = kfifo_len(&thread->signal_queue);
}

static void check_pending_signals(void)
{
    if (running_thread->signal_cnt == 0 ||
        running_thread->stack_top_preserved) {
        return;
    }

    /* Retrieve a pending signal from the queue */
    struct staged_handler_info info;
    kfifo_out(&running_thread->signal_queue, &info,
              sizeof(struct staged_handler_info));

    /* Stage the signal handler into the thread stack */
    stage_temporary_handler(running_thread, info.func,
                            (uint32_t) sig_return_handler, info.args);

    /* Update the number of total pending signals */
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
    /* Remove the thread from the system */
    list_del(&thread->task_list);
    list_del(&thread->thread_list);
    if (thread != running_thread)
        list_del(&thread->list);
    thread->status = THREAD_TERMINATED;
    bitmap_clear_bit(bitmap_threads, thread->tid);

    /* Free the thread stack memory */
    free_pages((uint32_t) thread->stack,
               size_to_page_order(thread->stack_size));

    /* Remove the task from the system if it contains no more thread */
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

    /* Find the highest priority task inside the waiting list */
    struct thread_info *thread;
    list_for_each_entry (thread, wait_list, list) {
        if (thread->priority > highest_pri_thread->priority)
            highest_pri_thread = thread;
    }

    /* Wake up the thread by placing it to the ready list */
    list_move(&highest_pri_thread->list,
              &ready_list[highest_pri_thread->priority]);
    highest_pri_thread->status = THREAD_READY;
}

void wake_up_all(struct list_head *wait_list)
{
    /* Wake up all threads from the waiting list */
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
        (unsigned long *) running_thread->stack_top_preserved;
    running_thread->stack_top_preserved = (unsigned long) NULL;
}

static inline void thread_join_handler(void)
{
    /* Wake up the threads that waiting to join */
    struct list_head *curr, *next;
    list_for_each_safe (curr, next, &running_thread->join_list) {
        struct thread_info *thread = list_entry(curr, struct thread_info, list);

        /* Pass the return value back to the joining thread */
        if (thread->retval_join) {
            *thread->retval_join = running_thread->retval;
        }

        finish_wait(&thread->list);
    }

    /* Remove the task from the system if it contains no more thread */
    struct task_struct *task = current_task_info();
    if (list_empty(&task->threads_list)) {
        /* Remove the task from the system */
        task_delete(task);
    }

    /* Remove the thread from the system */
    list_del(&running_thread->thread_list);
    list_del(&running_thread->task_list);
    running_thread->status = THREAD_TERMINATED;
    bitmap_clear_bit(bitmap_threads, running_thread->tid);

    /* Free the thread stack memory */
    free_pages((uint32_t) running_thread->stack,
               size_to_page_order(running_thread->stack_size));
}

static struct thread_info *thread_info_find_next(struct thread_info *curr)
{
    struct thread_info *thread = NULL;
    int tid = ((uintptr_t) curr - (uintptr_t) &threads[0]) /
              sizeof(struct thread_info);

    /* Find the next thread */
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
    /* Read syscall arguments */
    struct thread_stat *info = SYSCALL_ARG(struct thread_stat *, 0);
    void *next = SYSCALL_ARG(void *, 1);

    int tid;
    struct thread_info *thread = NULL;

    if (next == NULL) {
        /* Assign the first thread */
        tid = find_first_bit(bitmap_threads, THREAD_CNT_MAX);
        thread = &threads[tid];
    } else {
        /* Don't use thread->tid as the thread may be terminated */
        tid = ((uintptr_t) next - (uintptr_t) &threads[0]) /
              sizeof(struct thread_info);

        /* Check if the thread is still alive */
        if (bitmap_get_bit(bitmap_threads, tid)) {
            /* The thread is still alive */
            thread = (struct thread_info *) next;
        } else {
            /* The thread is not alive, find the next one that is alive */
            thread = thread_info_find_next(next);

            /* No more alive thread */
            if (!thread) {
                SYSCALL_ARG(void *, 0) = NULL;
                return;
            }
        }
    }

    /* Return thread information */
    info->pid = thread->task->pid;
    info->tid = thread->tid;
    info->priority = thread->priority;
    info->privileged = thread->privilege == KERNEL_THREAD;
    info->stack_usage =
        (size_t) ((uintptr_t) thread->stack + thread->stack_size -
                  (uintptr_t) thread->stack_top);
    info->stack_size = thread->stack_size;
    strncpy(info->name, thread->name, THREAD_NAME_MAX);

    switch (thread->status) {
    case THREAD_WAIT:
        info->status = "S";
        break;
    case THREAD_SUSPENDED:
        info->status = "T";
        break;
    case THREAD_READY:
    case THREAD_RUNNING:
        info->status = "R";
        break;
    default:
        info->status = "?";
        break;
    }

    /* Return the pointer of the next thread */
    SYSCALL_ARG(void *, 0) = thread_info_find_next(thread);
}

void sys_setprogname(void)
{
    /* Read syscall arguments */
    char *name = SYSCALL_ARG(char *, 0);

    strncpy(running_thread->name, name, THREAD_NAME_MAX);
}

void sys_delay_ticks(void)
{
    /* Read syscall arguments */
    uint32_t tick = SYSCALL_ARG(uint32_t, 0);

    /* Reconfigure the tick to sleep */
    running_thread->sleep_ticks = tick;

    /* Enqueue the thread into the sleep list */
    running_thread->status = THREAD_WAIT;
    list_add(&(running_thread->list), &sleep_list);

    /* Return success */
    SYSCALL_ARG(int, 0) = 0;
}

void sys_task_create(void)
{
    /* Read syscall arguments */
    task_func_t task_func = SYSCALL_ARG(task_func_t, 0);
    int priority = SYSCALL_ARG(int, 1);
    int stack_size = SYSCALL_ARG(int, 2);

    int retval = _task_create(task_func, priority, stack_size, USER_THREAD);
    if (retval < 0)
        printk("task_create(): failed to create new task");

    /* Return task creation result */
    SYSCALL_ARG(int, 0) = retval;
}

void sys_mpool_alloc(void)
{
    /* Read syscall arguments */
    struct mpool *mpool = SYSCALL_ARG(struct mpool *, 0);
    size_t size = SYSCALL_ARG(size_t, 1);

    void *ptr = NULL;
    size_t alloc_size = ALIGN(size, sizeof(long));

    /* Check if the memory poll has enough space */
    if ((mpool->offset + alloc_size) <= mpool->size) {
        ptr = (void *) ((uintptr_t) mpool->mem + mpool->offset);
        mpool->offset += alloc_size;
    }

    /* Return the allocated memory */
    SYSCALL_ARG(void *, 0) = ptr;
}

void sys_minfo(void)
{
    /* Read syscall arguments */
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

    /* Return the inquired information */
    SYSCALL_ARG(int, 0) = retval;
}

void sys_sched_yield(void)
{
    /* Suspend current thread */
    prepare_to_wait(&sleep_list, &running_thread->list, THREAD_WAIT);
}

void sys_exit(void)
{
    /* Read syscall arguments */
    int status = SYSCALL_ARG(int, 0);

    /* Acquire the current running task */
    struct task_struct *task = current_task_info();

    printk("exit(): task terminated (name: %s, pid: %d, status: %d)",
           task->main_thread->name, task->pid, status);

    /* Remove all threads of the task from the system */
    struct list_head *curr, *next;
    list_for_each_safe (curr, next, &task->threads_list) {
        struct thread_info *thread =
            list_entry(curr, struct thread_info, task_list);

        /* Remove current thread of iteration from the system */
        list_del(&thread->thread_list);
        list_del(&thread->task_list);
        list_del(&thread->list);
        thread->status = THREAD_TERMINATED;
        bitmap_clear_bit(bitmap_threads, thread->tid);

        /* Free the stack memory */
        free_pages((uint32_t) thread->stack,
                   size_to_page_order(thread->stack_size));
    }

    /* Remove the task from the system */
    task_delete(task);
}

void sys_mount(void)
{
    /* Read syscall arguments */
    char *source = SYSCALL_ARG(char *, 0);
    char *target = SYSCALL_ARG(char *, 1);

    int tid = running_thread->tid;

    /* Send mount request to the file system daemon */
    if (running_thread->syscall_pending == false) {
        request_mount(tid, source, target);
    }

    /* Read mount result from the file system daemon */
    int mnt_result;
    int retval =
        fifo_read(files[tid], (char *) &mnt_result, sizeof(mnt_result), 0);

    /* Check if the syscall need to be restarted */
    if (retval == -ERESTARTSYS) {
        /* The request is not yet processed */
        set_syscall_pending(running_thread);
    } else {
        /* The request is complete */
        reset_syscall_pending(running_thread);
        SYSCALL_ARG(int, 0) = mnt_result;
    }
}

void sys_open(void)
{
    /* Read syscall argument */
    char *pathname = SYSCALL_ARG(char *, 0);
    int flags = SYSCALL_ARG(int, 1);

    /* Acquire the running task */
    struct task_struct *task = current_task_info();

    /* Acquire the thread ID */
    int tid = running_thread->tid;

    /* Send file open request to the file system daemon */
    if (running_thread->syscall_pending == false) {
        request_open_file(tid, pathname);
    }

    /* Read the file index from the file system daemon */
    int file_idx;
    int retval = fifo_read(files[tid], (char *) &file_idx, sizeof(file_idx), 0);

    /* Check if the syscall need to be restarted */
    if (retval == -ERESTARTSYS) {
        /* The request is not yet processed */
        set_syscall_pending(running_thread);
        return;
    } else {
        /* The request is complete */
        reset_syscall_pending(running_thread);
    }

    /* File not found */
    if (file_idx == -1) {
        /* Return error */
        SYSCALL_ARG(int, 0) = -1;
        return;
    }

    /* Find a free entry on the file descriptor entry table */
    int fdesc_idx = find_first_zero_bit(bitmap_fds, FILE_DESC_CNT_MAX);
    if (fdesc_idx >= FILE_DESC_CNT_MAX) {
        /* Return error */
        SYSCALL_ARG(int, 0) = -ENOMEM;
        return;
    }
    bitmap_set_bit(bitmap_fds, fdesc_idx);
    bitmap_set_bit(task->bitmap_fds, fdesc_idx);

    struct file *filp = files[file_idx];

    /* Register new file descriptor on the table */
    struct fdtable *fdesc = &fdtable[fdesc_idx];
    fdesc->file = filp;
    fdesc->flags = flags;

    /* Check if the file operation is undefined */
    if (!filp->f_op->open) {
        /* Release the file descriptor */
        bitmap_clear_bit(bitmap_fds, fdesc_idx);
        bitmap_clear_bit(task->bitmap_fds, fdesc_idx);

        /* Return error */
        SYSCALL_ARG(int, 0) = -ENXIO;
        return;
    }

    /* Call open operation  */
    filp->f_op->open(filp->f_inode, filp);

    /* Return the file descriptor number */
    int fd = fdesc_idx + TASK_CNT_MAX;
    SYSCALL_ARG(int, 0) = fd;
}

void sys_close(void)
{
    /* Read syscall arguments */
    int fd = SYSCALL_ARG(int, 0);

    /* Acquire the running task */
    struct task_struct *task = current_task_info();

    /* Invalid file descriptor number */
    if (fd < TASK_CNT_MAX) {
        SYSCALL_ARG(int, 0) = -EBADF;
        return;
    }

    /* Calculate the index number of the file descriptor */
    int fdesc_idx = fd - TASK_CNT_MAX;

    /* Check if the file descriptor belongs to current task */
    if (!bitmap_get_bit(bitmap_fds, fdesc_idx) ||
        !bitmap_get_bit(task->bitmap_fds, fdesc_idx)) {
        SYSCALL_ARG(int, 0) = -EBADF;
        return;
    }

    /* Free the file descriptor */
    bitmap_clear_bit(bitmap_fds, fdesc_idx);
    bitmap_clear_bit(task->bitmap_fds, fdesc_idx);

    /* Return success */
    SYSCALL_ARG(int, 0) = 0;
}

void sys_dup(void)
{
    /* Read syscall arguments */
    int oldfd = SYSCALL_ARG(int, 0);

    if (oldfd < TASK_CNT_MAX) {
        SYSCALL_ARG(int, 0) = -EBADF;
        return;
    }

    /* Convert oldfd to the index numbers on the table */
    int old_fdesc_idx = oldfd - TASK_CNT_MAX;

    /* Acquire the running task */
    struct task_struct *task = current_task_info();

    /* Check if the file descriptor is invalid */
    if (!bitmap_get_bit(bitmap_fds, old_fdesc_idx) ||
        !bitmap_get_bit(task->bitmap_fds, old_fdesc_idx)) {
        SYSCALL_ARG(ssize_t, 0) = -EBADF;
        return;
    }

    /* Find a free entry on the file descriptor table */
    int fdesc_idx = find_first_zero_bit(bitmap_fds, FILE_DESC_CNT_MAX);
    if (fdesc_idx >= FILE_DESC_CNT_MAX) {
        /* Return error */
        SYSCALL_ARG(int, 0) = -ENOMEM;
        return;
    }
    bitmap_set_bit(bitmap_fds, fdesc_idx);
    bitmap_set_bit(task->bitmap_fds, fdesc_idx);

    /* Copy the old file descriptor content to the new one */
    fdtable[fdesc_idx] = fdtable[old_fdesc_idx];

    /* Return new file descriptor number */
    int fd = fdesc_idx + TASK_CNT_MAX;
    SYSCALL_ARG(int, 0) = fd;
}

void sys_dup2(void)
{
    /* read syscall arguments */
    int oldfd = SYSCALL_ARG(int, 0);
    int newfd = SYSCALL_ARG(int, 1);

    /* Convert fds to the index numbers on the table */
    int old_fdesc_idx = oldfd - TASK_CNT_MAX;
    int new_fdesc_idx = newfd - TASK_CNT_MAX;

    if (oldfd < TASK_CNT_MAX || newfd < TASK_CNT_MAX) {
        SYSCALL_ARG(int, 0) = -EBADF;
        return;
    }

    /* Acquire the running task */
    struct task_struct *task = current_task_info();

    /* Check if the file descriptors are invalid */
    if (!bitmap_get_bit(bitmap_fds, old_fdesc_idx) ||
        !bitmap_get_bit(bitmap_fds, new_fdesc_idx) ||
        !bitmap_get_bit(task->bitmap_fds, old_fdesc_idx) ||
        !bitmap_get_bit(task->bitmap_fds, new_fdesc_idx)) {
        SYSCALL_ARG(ssize_t, 0) = -EBADF;
        return;
    }

    /* Copy the old file descriptor content to the new one */
    fdtable[new_fdesc_idx] = fdtable[old_fdesc_idx];

    /* Return new file descriptor */
    SYSCALL_ARG(int, 0) = newfd;
}

void sys_read(void)
{
    /* Read syscall argument */
    int fd = SYSCALL_ARG(int, 0);
    char *buf = SYSCALL_ARG(char *, 1);
    size_t count = SYSCALL_ARG(size_t, 2);

    /* Acquire the running task */
    struct task_struct *task = current_task_info();

    /* Get the file to read */
    struct file *filp;
    if (fd < TASK_CNT_MAX) {
        /* Read target is the anonymous pipe of a thread */
        filp = files[fd];
    } else {
        /* Calculate the index number of the file descriptor
         * on the table */
        int fdesc_idx = fd - TASK_CNT_MAX;

        /* Check if the file descriptor is invalid */
        if (!bitmap_get_bit(bitmap_fds, fdesc_idx) ||
            !bitmap_get_bit(task->bitmap_fds, fdesc_idx)) {
            SYSCALL_ARG(ssize_t, 0) = -EBADF;
            return;
        }

        filp = fdtable[fdesc_idx].file;
        filp->f_flags = fdtable[fdesc_idx].flags;
    }

    /* Check if the file operation is undefined */
    if (!filp->f_op->read) {
        /* Return error */
        SYSCALL_ARG(ssize_t, 0) = -ENXIO;
        return;
    }

    /* Call read operation */
    ssize_t retval = filp->f_op->read(filp, buf, count, 0);

    /* Check if the syscall need to be restarted */
    if (retval == -ERESTARTSYS) {
        /* Reading is not yet finish */
        set_syscall_pending(running_thread);
    } else {
        /* Reading is finish, return the result */
        reset_syscall_pending(running_thread);
        SYSCALL_ARG(ssize_t, 0) = retval;
    }
}

void sys_write(void)
{
    /* Read syscall arguments */
    int fd = SYSCALL_ARG(int, 0);
    char *buf = SYSCALL_ARG(char *, 1);
    size_t count = SYSCALL_ARG(size_t, 2);

    /* Acquire the running task */
    struct task_struct *task = current_task_info();

    /* Get the file pointer */
    struct file *filp;
    if (fd < TASK_CNT_MAX) {
        /* Write target is the anonymous pipe of a thread */
        filp = files[fd];
    } else {
        /* Calculate the index number of the file descriptor
         * on the table */
        int fdesc_idx = fd - TASK_CNT_MAX;

        /* Check if the file descriptor is invalid */
        if (!bitmap_get_bit(bitmap_fds, fdesc_idx) ||
            !bitmap_get_bit(task->bitmap_fds, fdesc_idx)) {
            SYSCALL_ARG(ssize_t, 0) = -EBADF;
            return;
        }

        filp = fdtable[fdesc_idx].file;
        filp->f_flags = fdtable[fdesc_idx].flags;
    }

    /* Check if the file operation is undefined */
    if (!filp->f_op->write) {
        /* Return error */
        SYSCALL_ARG(ssize_t, 0) = -ENXIO;
        return;
    }

    /* Call write operation */
    ssize_t retval = filp->f_op->write(filp, buf, count, 0);

    /* Check if the syscall need to be restarted */
    if (retval == -ERESTARTSYS) {
        /* Writing is not yet finish */
        set_syscall_pending(running_thread);
    } else {
        /* Writing is complete, return the result */
        reset_syscall_pending(running_thread);
        SYSCALL_ARG(ssize_t, 0) = retval;
    }
}

void sys_ioctl(void)
{
    /* Read syscall arguments */
    int fd = SYSCALL_ARG(int, 0);
    unsigned int cmd = SYSCALL_ARG(unsigned int, 1);
    unsigned long arg = SYSCALL_ARG(unsigned long, 2);

    /* Acquire the running task */
    struct task_struct *task = current_task_info();

    /* Get the file pointer */
    struct file *filp;
    if (fd < TASK_CNT_MAX) {
        /* I/O control target is the anonymous pipe of a thread */
        filp = files[fd];
    } else {
        /* calculate the index number of the file descriptor
         * on the table */
        int fdesc_idx = fd - TASK_CNT_MAX;

        /* Check if the file descriptor is invalid */
        if (!bitmap_get_bit(bitmap_fds, fdesc_idx) ||
            !bitmap_get_bit(task->bitmap_fds, fdesc_idx)) {
            SYSCALL_ARG(int, 0) = -EBADF;
            return;
        }

        filp = fdtable[fdesc_idx].file;
    }

    /* Check if the file operation is undefined */
    if (!filp->f_op->ioctl) {
        /* Return error */
        SYSCALL_ARG(int, 0) = -ENXIO;
        return;
    }

    /* Call ioctl operation */
    int retval = filp->f_op->ioctl(filp, cmd, arg);

    /* Check if the syscall need to be restarted */
    if (retval == -ERESTARTSYS) {
        /* I/O control is not yet finish */
        set_syscall_pending(running_thread);
    } else {
        /* I/O control is finish, return the result */
        reset_syscall_pending(running_thread);
        SYSCALL_ARG(int, 0) = retval;
    }
}

void sys_lseek(void)
{
    /* Read syscall arguments */
    int fd = SYSCALL_ARG(int, 0);
    long offset = SYSCALL_ARG(long, 1);
    int whence = SYSCALL_ARG(int, 2);

    /* Acquire the running task */
    struct task_struct *task = current_task_info();

    /* Get the file pointer */
    struct file *filp;
    if (fd < TASK_CNT_MAX) {
        /* lseek target is the anonymous pipe of a thread */
        filp = files[fd];
    } else {
        /* Calculate the index number of the file descriptor
         * on the table */
        int fdesc_idx = fd - TASK_CNT_MAX;

        /* Check if the file descriptor is invalid */
        if (!bitmap_get_bit(bitmap_fds, fdesc_idx) ||
            !bitmap_get_bit(task->bitmap_fds, fdesc_idx)) {
            SYSCALL_ARG(off_t, 0) = -EBADF;
            return;
        }

        filp = fdtable[fdesc_idx].file;
    }

    /* Check if the file operation is undefined */
    if (!filp->f_op->lseek) {
        /* Return error */
        SYSCALL_ARG(off_t, 0) = -ENXIO;
        return;
    }

    /* Call lseek operation */
    int retval = filp->f_op->lseek(filp, offset, whence);

    /* check if the syscall need to be restarted */
    if (retval == -ERESTARTSYS) {
        /* lseek is not yet finish */
        set_syscall_pending(running_thread);
    } else {
        /* lseek is finish, return the result */
        reset_syscall_pending(running_thread);
        SYSCALL_ARG(off_t, 0) = retval;
    }
}

void sys_fstat(void)
{
    /* Read syscall arguments */
    int fd = SYSCALL_ARG(int, 0);
    struct stat *statbuf = SYSCALL_ARG(struct stat *, 1);

    /* Acquire the running task */
    struct task_struct *task = current_task_info();

    if (fd < TASK_CNT_MAX) {
        SYSCALL_ARG(int, 0) = -EBADF;
        return;
    }

    /* Calculate the index number of the file descriptor
     * on the table */
    int fdesc_idx = fd - TASK_CNT_MAX;

    /* Check if the file descriptor is invalid */
    if (!bitmap_get_bit(bitmap_fds, fdesc_idx) ||
        !bitmap_get_bit(task->bitmap_fds, fdesc_idx)) {
        SYSCALL_ARG(int, 0) = -EBADF;
        return;
    }

    /* Get file inode */
    struct inode *inode = fdtable[fdesc_idx].file->f_inode;

    /* Check if the inode exists */
    if (inode != NULL) {
        statbuf->st_mode = inode->i_mode;
        statbuf->st_ino = inode->i_ino;
        statbuf->st_rdev = inode->i_rdev;
        statbuf->st_size = inode->i_size;
        statbuf->st_blocks = inode->i_blocks;
    }

    /* Return success */
    SYSCALL_ARG(int *, 0) = 0;
}

void sys_opendir(void)
{
    /* Read syscall arguments */
    char *pathname = SYSCALL_ARG(char *, 0);
    DIR *dirp = SYSCALL_ARG(DIR *, 1);

    int tid = running_thread->tid;

    /* Send directory open request to the file system daemon */
    if (running_thread->syscall_pending == false) {
        request_open_directory(tid, pathname);
    }

    /* Read the directory inode from the file system daemon */
    struct inode *inode_dir;
    int retval =
        fifo_read(files[tid], (char *) &inode_dir, sizeof(inode_dir), 0);

    /* Check if the syscall need to be restarted */
    if (retval == -ERESTARTSYS) {
        /* The request if not yet processed */
        set_syscall_pending(running_thread);
        return;
    } else {
        /* The request is complete */
        reset_syscall_pending(running_thread);
    }

    /* Return directory information */
    dirp->inode_dir = inode_dir;
    dirp->dentry_list = inode_dir->i_dentry.next;

    /* Check if the information is retrieved successfully */
    if (dirp->inode_dir == NULL) {
        /* Return error */
        SYSCALL_ARG(int, 0) = -1;
    } else {
        /* Return sucesss */
        SYSCALL_ARG(int, 0) = 0;
    }
}

void sys_readdir(void)
{
    /* Read syscall arguments */
    DIR *dirp = SYSCALL_ARG(DIR *, 0);
    struct dirent *dirent = SYSCALL_ARG(struct dirent *, 1);

    /* Return the direcotry entry structure */
    SYSCALL_ARG(int, 0) = (uint32_t) fs_read_dir(dirp, dirent);
}

void sys_getcwd(void)
{
    /* Read syscall arguments */
    char *buf = SYSCALL_ARG(char *, 0);
    size_t len = SYSCALL_ARG(size_t, 1);

    int tid = running_thread->tid;

    /* Send getcwd request to the file system daemon */
    if (running_thread->syscall_pending == false) {
        request_getcwd(tid, buf, len);
    }

    /* Read getcwd result from the file system daemon */
    char *path;
    int retval = fifo_read(files[tid], (char *) &path, sizeof(path), 0);

    /* Check if the syscall need to be restarted */
    if (retval == -ERESTARTSYS) {
        /* The request is not yet processed */
        set_syscall_pending(running_thread);
    } else {
        /* The request is complete, return current pathname */
        reset_syscall_pending(running_thread);
        SYSCALL_ARG(char *, 0) = path;
    }
}

void sys_chdir(void)
{
    /* Read syscall arguments */
    char *path = SYSCALL_ARG(char *, 0);

    int tid = running_thread->tid;

    /* Send chdir request to the file system daemon */
    if (running_thread->syscall_pending == false) {
        request_chdir(tid, path);
    }

    /* Read chdir result from the file system daemon */
    int chdir_result;
    int retval =
        fifo_read(files[tid], (char *) &chdir_result, sizeof(chdir_result), 0);

    /* Check if the syscall need to be restarted */
    if (retval == -ERESTARTSYS) {
        /* The request is not yet processed */
        set_syscall_pending(running_thread);
    } else {
        /* The request is complete, return the result */
        reset_syscall_pending(running_thread);
        SYSCALL_ARG(int, 0) = chdir_result;
    }
}

void sys_getpid(void)
{
    /* Return the task ID */
    struct task_struct *task = current_task_info();
    SYSCALL_ARG(int, 0) = task->pid;
}

void sys_mknod(void)
{
    /* Read syscall arguments */
    char *pathname = SYSCALL_ARG(char *, 0);
    // mode_t mode = SYSCALL_ARG(mode_t, 1);
    dev_t dev = SYSCALL_ARG(dev_t, 2);

    int tid = running_thread->tid;

    /* Send file create request to the file system daemon */
    if (running_thread->syscall_pending == false) {
        request_create_file(tid, pathname, dev);
    }

    /* Read file index from the file system daemon  */
    int file_idx;
    int retval = fifo_read(files[tid], (char *) &file_idx, sizeof(file_idx), 0);

    /* Check if the syscall need to be restarted */
    if (retval == -ERESTARTSYS) {
        /* The request is not yet processed */
        set_syscall_pending(running_thread);
        return;
    } else {
        /* The request is complete */
        reset_syscall_pending(running_thread);
    }

    if (file_idx == -1) {
        /* File not found, return error */
        SYSCALL_ARG(int, 0) = -1;
    } else {
        /* Return sucesss */
        SYSCALL_ARG(int, 0) = 0;
    }
}

void sys_mkfifo(void)
{
    /* Read syscall arguments */
    char *pathname = SYSCALL_ARG(char *, 0);
    // mode_t mode = SYSCALL_ARG(mode_t, 1);

    int tid = running_thread->tid;

    /* Send file create request to the file system daemon */
    if (running_thread->syscall_pending == false) {
        request_create_file(tid, pathname, S_IFIFO);
    }

    /* Read the file index from the file system daemon */
    int file_idx = 0;
    int retval = fifo_read(files[tid], (char *) &tid, sizeof(file_idx), 0);

    /* Check if the syscall need to be restarted */
    if (retval == -ERESTARTSYS) {
        /* The request is not yet processed */
        set_syscall_pending(running_thread);
        return;
    } else {
        /* The request is complete */
        reset_syscall_pending(running_thread);
    }

    if (file_idx == -1) {
        /* File not found, return error */
        SYSCALL_ARG(int, 0) = -1;
    } else {
        /* Return sucesss */
        SYSCALL_ARG(int, 0) = 0;
    }
}

void poll_notify(struct file *notify_file)
{
    /* Iterate through all threads suspended by the poll() syscall */
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
    /* Read syscall arguments */
    struct pollfd *fds = SYSCALL_ARG(struct pollfd *, 0);
    nfds_t nfds = SYSCALL_ARG(nfds_t, 1);
    int timeout = SYSCALL_ARG(int, 2);

    if (running_thread->syscall_pending == false) {
        /* Set polling deadline */
        if (timeout > 0) {
            struct timespec tp;
            get_sys_time(&tp);
            time_add(&tp, 0, timeout * 1000000);
            running_thread->syscall_timeout = tp;
        }

        /* Initialize the polling file list */
        INIT_LIST_HEAD(&running_thread->poll_files_list);
        running_thread->syscall_is_timeout = false;

        /* Check file events */
        struct file *filp;
        bool no_event = true;

        for (int i = 0; i < nfds; i++) {
            int fd = fds[i].fd - TASK_CNT_MAX;
            filp = fdtable[fd].file;

            uint32_t events = filp->f_events;
            if (~fds[i].events & events) {
                no_event = false;
            }

            /* Return file events */
            fds[i].revents |= events;
        }

        if (!no_event) {
            /* Return sucesss */
            SYSCALL_ARG(int, 0) = 0;
            return;
        }

        /* No events is observed and no timeout is set, return immediately */
        if (timeout == 0) {
            /* return error */
            SYSCALL_ARG(int, 0) = -1;
            return;
        }

        /* Turn on the syscall pending flag */
        set_syscall_pending(running_thread);

        /* Suspend current thread */
        prepare_to_wait(&poll_list, &running_thread->list, THREAD_WAIT);

        /* Add current thread into the timeout monitoring list */
        if (timeout > 0)
            list_add(&running_thread->timeout_list, &timeout_list);

        /* Record all files for polling */
        for (int i = 0; i < nfds; i++) {
            int fd = fds[i].fd - TASK_CNT_MAX;
            filp = fdtable[fd].file;
            list_add(&filp->list, &running_thread->poll_files_list);
        }
    } else {
        /* Turn off the syscall pending flag */
        reset_syscall_pending(running_thread);

        /* clear list of poll files */
        INIT_LIST_HEAD(&running_thread->poll_files_list);

        /* Remove the thread from the polling list */
        if (timeout > 0)
            list_del(&running_thread->timeout_list);

        if (running_thread->syscall_is_timeout) {
            /* Return error */
            SYSCALL_ARG(int, 0) = -1;
        } else {
            /* Return sucesss */
            SYSCALL_ARG(int, 0) = 0;
        }
    }
}

void sys_mq_getattr(void)
{
    /* Read syscall arguments */
    mqd_t mqdes = SYSCALL_ARG(mqd_t, 0);
    struct mq_attr *attr = SYSCALL_ARG(struct mq_attr *, 1);

    /* Check if the message queue descriptor is invalid */
    struct task_struct *task = current_task_info();
    if (!bitmap_get_bit(bitmap_mqds, mqdes) ||
        !bitmap_get_bit(task->bitmap_mqds, mqdes)) {
        SYSCALL_ARG(long, 0) = -EBADF;
        return;
    }

    /* Return attributes */
    attr->mq_flags = mqd_table[mqdes].attr.mq_flags;
    attr->mq_maxmsg = mqd_table[mqdes].attr.mq_maxmsg;
    attr->mq_msgsize = mqd_table[mqdes].attr.mq_msgsize;
    attr->mq_curmsgs = kfifo_len(mqd_table[mqdes].mq->pipe->fifo);

    /* Return sucesss */
    SYSCALL_ARG(int, 0) = 0;
}

void sys_mq_setattr(void)
{
    /* Read syscall arguments */
    mqd_t mqdes = SYSCALL_ARG(mqd_t, 0);
    struct mq_attr *newattr = SYSCALL_ARG(struct mq_attr *, 1);
    struct mq_attr *oldattr = SYSCALL_ARG(struct mq_attr *, 2);

    struct task_struct *task = current_task_info();
    if (!bitmap_get_bit(bitmap_mqds, mqdes) ||
        !bitmap_get_bit(task->bitmap_mqds, mqdes)) {
        SYSCALL_ARG(long, 0) = -EBADF;
        return;
    }

    /* Acquire message queue attributes from the table */
    struct mq_attr *curr_attr = &mqd_table[mqdes].attr;

    /* Only the O_NONBLOCK bit-field in mq_flags can be changed */
    if (newattr->mq_flags & ~O_NONBLOCK ||
        newattr->mq_maxmsg != curr_attr->mq_maxmsg ||
        newattr->mq_msgsize != curr_attr->mq_msgsize) {
        /* Return error */
        SYSCALL_ARG(int, 0) = -EINVAL;
        return;
    }

    /* Preserve old attributes */
    oldattr->mq_flags = curr_attr->mq_flags;
    oldattr->mq_maxmsg = curr_attr->mq_maxmsg;
    oldattr->mq_msgsize = curr_attr->mq_msgsize;
    oldattr->mq_curmsgs = kfifo_len(mqd_table[mqdes].mq->pipe->fifo);

    /* Save new mq_flags attribute */
    curr_attr->mq_flags = (~O_NONBLOCK) & newattr->mq_flags;

    /* Return sucesss */
    SYSCALL_ARG(int, 0) = 0;
}

struct mqueue *acquire_mqueue(char *name)
{
    /* Find the message queue with the given name */
    struct mqueue *mq;
    list_for_each_entry (mq, &mqueue_list, list) {
        if (!strncmp(name, mq->name, FILE_NAME_LEN_MAX))
            return mq; /* Found */
    }

    /* Not found */
    return NULL;
}

void sys_mq_open(void)
{
    /* Read syscall arguments */
    char *name = SYSCALL_ARG(char *, 0);
    int oflag = SYSCALL_ARG(int, 1);
    struct mq_attr *attr = SYSCALL_ARG(struct mq_attr *, 2);

    /* Acquire the running task */
    struct task_struct *task = current_task_info();

    /* Search the message with the given name */
    struct mqueue *mq = acquire_mqueue(name);

    /* Check if new message queue descriptor can be dispatched */
    int mqdes = find_first_zero_bit(bitmap_mqds, MQUEUE_CNT_MAX);
    if (mqdes >= MQUEUE_CNT_MAX) {
        SYSCALL_ARG(mqd_t, 0) = -ENOMEM;
        return;
    }

    /* Attribute is not provided */
    struct mq_attr default_attr;
    if (!attr) {
        /* Use default attributes */
        default_attr.mq_maxmsg = 16;
        default_attr.mq_msgsize = 50;
        attr = &default_attr;
    }

    /* Message queue with specified name exists */
    if (mq) {
        /* Both O_CREAT and O_EXCL flags are set */
        if (oflag & O_CREAT && oflag & O_EXCL) {
            /* Return error */
            SYSCALL_ARG(mqd_t, 0) = -EEXIST;
            return;
        }

        /* Register new message queue descriptor */
        bitmap_set_bit(bitmap_mqds, mqdes);
        bitmap_set_bit(task->bitmap_mqds, mqdes);
        mqd_table[mqdes].mq = mq;
        mqd_table[mqdes].attr = *attr;
        mqd_table[mqdes].attr.mq_flags = oflag;

        /* Return message queue descriptor */
        SYSCALL_ARG(mqd_t, 0) = mqdes;
        return;
    }

    /* O_CREAT flag is not set */
    if (!(oflag & O_CREAT)) {
        /* Return error */
        SYSCALL_ARG(mqd_t, 0) = -ENOENT;
        return;
    }

    /* Allocate new message queue */
    struct pipe *pipe = __mq_open(attr);
    struct mqueue *new_mq = kmalloc(sizeof(struct mqueue));

    /* Memory allocation failure */
    if (!pipe || !new_mq) {
        /* Return error */
        SYSCALL_ARG(mqd_t, 0) = -ENOMEM;
        return;
    }

    /* Set up the message queue */
    new_mq->pipe = pipe;
    strncpy(new_mq->name, name, FILE_NAME_LEN_MAX);
    list_add(&new_mq->list, &mqueue_list);

    /* Register new message queue descriptor */
    bitmap_set_bit(bitmap_mqds, mqdes);
    bitmap_set_bit(task->bitmap_mqds, mqdes);
    mqd_table[mqdes].mq = new_mq;
    mqd_table[mqdes].attr = *attr;
    mqd_table[mqdes].attr.mq_flags = oflag;

    /* Return the message queue descriptor */
    SYSCALL_ARG(mqd_t, 0) = mqdes;
}

void sys_mq_close(void)
{
    /* Read syscall arguments */
    mqd_t mqdes = SYSCALL_ARG(mqd_t, 0);

    /* Check if the message queue descriptor is invalid */
    struct task_struct *task = current_task_info();
    if (!bitmap_get_bit(bitmap_mqds, mqdes) ||
        !bitmap_get_bit(task->bitmap_mqds, mqdes)) {
        SYSCALL_ARG(long, 0) = -EBADF;
        return;
    }

    /* Free the message queue descriptor */
    bitmap_clear_bit(bitmap_mqds, mqdes);
    bitmap_clear_bit(task->bitmap_mqds, mqdes);

    /* Return sucesss */
    SYSCALL_ARG(long, 0) = 0;
}

void sys_mq_unlink(void)
{
    char *name = SYSCALL_ARG(char *, 0);

    // TODO: String length check

    /* Search the message with the given name */
    struct mqueue *mq = acquire_mqueue(name);

    /* Check if the message queue exists */
    if (mq) {
        /* Remove the message queue from the system */
        list_del(&mq->list);
        kfree(mq);

        /* Return sucesss */
        SYSCALL_ARG(int, 0) = 0;
    } else {
        /* Return error */
        SYSCALL_ARG(int, 0) = -ENOENT;
    }
}

void sys_mq_receive(void)
{
    /* Read syscall arguments */
    mqd_t mqdes = SYSCALL_ARG(mqd_t, 0);
    char *msg_ptr = SYSCALL_ARG(char *, 1);
    size_t msg_len = SYSCALL_ARG(size_t, 2);

    /* Check if the message queue descriptor is invalid */
    struct task_struct *task = current_task_info();
    if (!bitmap_get_bit(bitmap_mqds, mqdes) ||
        !bitmap_get_bit(task->bitmap_mqds, mqdes)) {
        SYSCALL_ARG(long, 0) = -EBADF;
        return;
    }

    /* Acquire the message queue */
    struct mqueue *mq = mqd_table[mqdes].mq;
    struct pipe *pipe = mq->pipe;

    /* Check if msg_len exceeds maximum size */
    if (msg_len > mqd_table[mqdes].attr.mq_msgsize) {
        SYSCALL_ARG(int, 0) = -EMSGSIZE;
        return;
    }

    /* Read message */
    ssize_t retval =
        __mq_receive(pipe, msg_ptr, msg_len, &mqd_table[mqdes].attr);

    /* Check if the syscall need to be restarted */
    if (retval == -ERESTARTSYS) {
        /* Reception is not yet finish */
        set_syscall_pending(running_thread);
    } else {
        /* Reception is finish, return the result */
        reset_syscall_pending(running_thread);
        SYSCALL_ARG(int, 0) = retval;
    }
}

void sys_mq_send(void)
{
    /* Read syscall arguments */
    mqd_t mqdes = SYSCALL_ARG(mqd_t, 0);
    char *msg_ptr = SYSCALL_ARG(char *, 1);
    size_t msg_len = SYSCALL_ARG(size_t, 2);

    /* Check if the message queue descriptor is invalid */
    struct task_struct *task = current_task_info();
    if (!bitmap_get_bit(bitmap_mqds, mqdes) ||
        !bitmap_get_bit(task->bitmap_mqds, mqdes)) {
        SYSCALL_ARG(int, 0) = -EBADF;
        return;
    }

    /* Acquire the message queue */
    struct mqueue *mq = mqd_table[mqdes].mq;
    struct pipe *pipe = mq->pipe;

    /* Check if msg_len exceeds maximum size */
    if (msg_len > mqd_table[mqdes].attr.mq_msgsize) {
        SYSCALL_ARG(int, 0) = -EMSGSIZE;
        return;
    }

    /* Send message */
    ssize_t retval = __mq_send(pipe, msg_ptr, msg_len, &mqd_table[mqdes].attr);

    /* Check if the syscall need to be restarted */
    if (retval == -ERESTARTSYS) {
        /* Sending is not finish  */
        set_syscall_pending(running_thread);
    } else {
        /* Sending is complete, return the result */
        reset_syscall_pending(running_thread);
        SYSCALL_ARG(int, 0) = retval;
    }
}

void sys_pthread_create(void)
{
    typedef void *(*start_routine_t)(void *);

    /* Read syscall arguments */
    pthread_t *pthread = SYSCALL_ARG(pthread_t *, 0);
    pthread_attr_t *_attr = SYSCALL_ARG(pthread_attr_t *, 1);
    start_routine_t start_routine = SYSCALL_ARG(start_routine_t, 2);
    void *arg = SYSCALL_ARG(void *, 3);

    struct thread_attr *attr = (struct thread_attr *) _attr;

    /* Use defualt attributes if user did not provide */
    struct thread_attr default_attr;
    if (attr == NULL) {
        pthread_attr_init((pthread_attr_t *) &default_attr);
        default_attr.schedparam.sched_priority = running_thread->priority;
        attr = &default_attr;
    }

    /* Create new thread */
    struct thread_info *thread;
    int retval = thread_create(&thread, (thread_func_t) start_routine, attr,
                               arg, USER_THREAD);
    if (retval != 0) {
        /* Failed to create new thread, return error */
        SYSCALL_ARG(int, 0) = retval;
        return;
    }

    strncpy(thread->name, running_thread->name, THREAD_NAME_MAX);

    /* Set task ownership to the thread */
    thread->task = current_task_info();
    list_add(&thread->task_list, &running_thread->task->threads_list);

    /* Return thread ID */
    *pthread = thread->tid;

    /* Return sucesss */
    SYSCALL_ARG(int, 0) = 0;
}

void sys_pthread_self(void)
{
    /* Return thread ID */
    SYSCALL_ARG(pthread_t, 0) = (pthread_t) running_thread->tid;
}

void sys_pthread_join(void)
{
    /* Read syscall arguments */
    pthread_t tid = SYSCALL_ARG(pthread_t, 0);
    void **retval = SYSCALL_ARG(void **, 1);

    struct thread_info *thread = acquire_thread(tid);
    if (!thread) {
        /* Return error */
        SYSCALL_ARG(int, 0) = -ESRCH;
        return;
    }

    if (thread->detached || !thread->joinable) {
        /* Return error */
        SYSCALL_ARG(int, 0) = -EINVAL;
        return;
    }

    /* Check deadlock (threads should not join on each other) */
    struct thread_info *check_thread;
    list_for_each_entry (check_thread, &running_thread->join_list, list) {
        /* Check if the thread is waiting to join the running thread */
        if (check_thread == thread) {
            /* Deadlock identified, return error */
            SYSCALL_ARG(int, 0) = -EDEADLK;
            return;
        }
    }

    running_thread->retval_join = retval;

    list_add(&running_thread->list, &thread->join_list);
    running_thread->status = THREAD_WAIT;

    /* Return sucesss (after the thread is awakened) */
    SYSCALL_ARG(int, 0) = 0;
}

void sys_pthread_cancel(void)
{
    /* Read syscall arguments */
    pthread_t tid = SYSCALL_ARG(pthread_t, 0);

    struct thread_info *thread = acquire_thread(tid);
    if (!thread) {
        /* Return error */
        SYSCALL_ARG(int, 0) = -ESRCH;
        return;
    }

    /* kthread can only be canceled by the kernel */
    if (thread->privilege == KERNEL_THREAD) {
        /* Return error */
        SYSCALL_ARG(int, 0) = -EPERM;
        return;
    }

    thread_delete(thread);

    /* Return sucesss */
    SYSCALL_ARG(int, 0) = 0;
}

void sys_pthread_detach(void)
{
    /* Read syscall arguments */
    pthread_t tid = SYSCALL_ARG(pthread_t, 0);

    /* Check if the thread exists */
    if (!bitmap_get_bit(bitmap_threads, tid)) {
        /* Return error */
        SYSCALL_ARG(long, 0) = -ESRCH;
        return;
    }

    threads[tid].detached = true;

    /* Return sucesss */
    SYSCALL_ARG(int, 0) = 0;
}

void sys_pthread_setschedparam(void)
{
    /* Read syscall arguments */
    pthread_t tid = SYSCALL_ARG(pthread_t, 0);
    // int policy = SYSCALL_ARG(int, 1);
    struct sched_param *param = SYSCALL_ARG(struct sched_param *, 2);

    struct thread_info *thread = acquire_thread(tid);
    if (!thread) {
        /* Return error */
        SYSCALL_ARG(int, 0) = -ESRCH;
        return;
    }

    /* kthread can only be configured by the kernel */
    if (thread->privilege == KERNEL_THREAD) {
        /* Return error */
        SYSCALL_ARG(int, 0) = -EPERM;
        return;
    }

    /* Invalid priority parameter */
    if (param->sched_priority < 0 ||
        param->sched_priority > THREAD_PRIORITY_MAX) {
        /* Return error */
        SYSCALL_ARG(int, 0) = -EINVAL;
        return;
    }

    /* Apply settings */
    if (thread->priority_inherited)
        thread->original_priority = param->sched_priority;
    else
        thread->priority = param->sched_priority;

    /* Return sucesss */
    SYSCALL_ARG(int, 0) = 0;
}

void sys_pthread_getschedparam(void)
{
    /* Read syscall arguments */
    pthread_t tid = SYSCALL_ARG(pthread_t, 0);
    // int *policy = SYSCALL_ARG(int *, 1);
    struct sched_param *param = SYSCALL_ARG(struct sched_param *, 2);

    struct thread_info *thread = acquire_thread(tid);
    if (!thread) {
        /* Return error */
        SYSCALL_ARG(int, 0) = -ESRCH;
        return;
    }

    /* kthread can only be set by the kernel */
    if (thread->privilege == KERNEL_THREAD) {
        /* Return error */
        SYSCALL_ARG(int, 0) = -EPERM;
        return;
    }

    /* Return settings */
    if (thread->priority_inherited)
        param->sched_priority = thread->original_priority;
    else
        param->sched_priority = thread->priority;

    /* Return sucesss */
    SYSCALL_ARG(int, 0) = 0;
}

void sys_pthread_yield(void)
{
    /* Yield the time quatum to other threads */
    prepare_to_wait(&sleep_list, &running_thread->list, THREAD_WAIT);
}

static void handle_signal(struct thread_info *thread, int signum)
{
    bool stage_handler = false;

    /* Wake up the thread from the signal waiting list */
    if (thread->wait_for_signal && (sig2bit(signum) & thread->sig_wait_set)) {
        /* Disable signal waiting flag */
        thread->wait_for_signal = false;

        /* Wake up the thread and set the return values */
        finish_wait(&thread->list);
        *thread->ret_sig = signum;
        *(int *) thread->syscall_args[0] = 0;
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
        /* Can't be caught or ignored */
        thread_suspend(thread);
        break;
    case SIGCONT:
        thread_resume(thread);
        stage_handler = true;
        break;
    case SIGKILL: {
        /* Can't be caught or ignored */
        thread_delete(thread);
        break;
    }
    }

    if (!stage_handler) {
        return;
    }

    int sig_idx = get_signal_index(signum);
    struct sigaction *act = thread->sig_table[sig_idx];

    /* Signal handler is not provided */
    if (act == NULL) {
        return;
    } else if (act->sa_handler == NULL) {
        return;
    }

    /* Stage signal or sigaction handler */
    if (act->sa_flags & SA_SIGINFO) {
        uint32_t args[4] = {0};
        args[0] = (uint32_t) signum;
        args[1] = (uint32_t) NULL /* info (TODO) */;
        args[2] = (uint32_t) NULL /* context (TODO) */;
        stage_signal_handler(thread, (uint32_t) act->sa_sigaction, args);
    } else {
        uint32_t args[4] = {0};
        args[0] = (uint32_t) signum;
        stage_signal_handler(thread, (uint32_t) act->sa_handler, args);
    }
}

void sys_pthread_kill(void)
{
    /* Read syscall arguments */
    pthread_t tid = SYSCALL_ARG(pthread_t, 0);
    int sig = SYSCALL_ARG(int, 1);

    struct thread_info *thread = acquire_thread(tid);

    /* Failed to find the thread */
    if (!thread) {
        /* Return error */
        SYSCALL_ARG(int, 0) = -ESRCH;
        return;
    }

    /* kthread does not receive signals */
    if (thread->privilege == KERNEL_THREAD) {
        /* Return error */
        SYSCALL_ARG(int, 0) = -EPERM;
        return;
    }

    /* Check if the signal number is defined */
    if (!is_signal_defined(sig)) {
        /* Return error */
        SYSCALL_ARG(int, 0) = -EINVAL;
        return;
    }

    handle_signal(thread, sig);

    /* Return sucesss */
    SYSCALL_ARG(int, 0) = 0;
}

void sys_pthread_exit(void)
{
    /* Read syscall arguments */
    void *retval = SYSCALL_ARG(void *, 0);
    running_thread->retval = retval;

    thread_join_handler();
}

void sys_pthread_mutex_unlock(void)
{
    /* Read syscall arguments */
    pthread_mutex_t *_mutex = SYSCALL_ARG(pthread_mutex_t *, 0);

    struct mutex *mutex = (struct mutex *) _mutex;
    int retval = mutex_unlock(mutex);

    /* Handle priority inheritance */
    if (mutex->protocol == PTHREAD_PRIO_INHERIT && retval == 0 &&
        running_thread->priority_inherited) {
        /* Recover the original thread priority */
        running_thread->priority = running_thread->original_priority;
    }

    SYSCALL_ARG(int, 0) = retval;
}

void sys_pthread_mutex_lock(void)
{
    /* Read syscall arguments */
    pthread_mutex_t *_mutex = SYSCALL_ARG(pthread_mutex_t *, 0);

    struct mutex *mutex = (struct mutex *) _mutex;
    int retval = mutex_lock(mutex);

    /* Check if the syscall need to be restarted */
    if (retval == -ERESTARTSYS) { /* Failed to acquire the mutex */
        /* Handle priority inheritance */
        if (mutex->protocol == PTHREAD_PRIO_INHERIT &&
            mutex->owner->priority < running_thread->priority) {
            /* Raise the priority of the owner thread */
            uint8_t old_priority = mutex->owner->priority;
            uint8_t new_priority = running_thread->priority;

            /* Move the owner thread from the ready list with lower priority to
             * the new list with raised priority  */
            struct list_head *curr, *next;
            list_for_each_safe (curr, next, &ready_list[old_priority]) {
                struct thread_info *thread =
                    list_entry(curr, struct thread_info, list);
                if (thread == mutex->owner) {
                    list_move(&thread->list, &ready_list[new_priority]);
                }
            }

            /* Set new priority and mark the priority inherited flag */
            mutex->owner->priority = new_priority;
            mutex->owner->priority_inherited = true;
        }

        /* Turn on the syscall pending flag */
        set_syscall_pending(running_thread);
    } else { /* Acquired the mutex successfully */
        /* Handle priority inheritance */
        if (mutex->protocol == PTHREAD_PRIO_INHERIT) {
            /* Preserve thread priority */
            running_thread->original_priority = running_thread->priority;
        }

        /* Turn off the syscall pending flag */
        reset_syscall_pending(running_thread);

        /* Return sucesss */
        SYSCALL_ARG(int, 0) = retval;
    }
}

void sys_pthread_mutex_trylock(void)
{
    /* Read syscall arguments */
    pthread_mutex_t *mutex = SYSCALL_ARG(pthread_mutex_t *, 0);

    int retval = mutex_lock((struct mutex *) mutex);

    /* Check if the syscall need to be restarted */
    if (retval == -ERESTARTSYS) {
        /* Failed to lock the mutex */
        SYSCALL_ARG(int, 0) = -EBUSY;
    } else {
        /* Return sucesss */
        SYSCALL_ARG(int, 0) = retval;
    }
}

void sys_pthread_cond_signal(void)
{
    /* Read syscall arguments */
    pthread_cond_t *cond = SYSCALL_ARG(pthread_cond_t *, 0);

    /* Wake up a thread from the wait list */
    wake_up(&((struct cond *) cond)->task_wait_list);

    /* pthread_cond_signal never returns error code */
    SYSCALL_ARG(int, 0) = 0;
}

void sys_pthread_cond_broadcast(void)
{
    /* Read syscall arguments */
    pthread_cond_t *cond = SYSCALL_ARG(pthread_cond_t *, 0);

    /* Wake up all threads from the wait list */
    wake_up_all(&((struct cond *) cond)->task_wait_list);

    /* pthread_cond_signal never returns error code */
    SYSCALL_ARG(int, 0) = 0;
}

void sys_pthread_cond_wait(void)
{
    /* Read syscall arguments */
    pthread_cond_t *cond = SYSCALL_ARG(pthread_cond_t *, 0);
    pthread_mutex_t *mutex = SYSCALL_ARG(pthread_mutex_t *, 1);

    if (((struct mutex *) mutex)->owner) {
        /* Release the mutex */
        ((struct mutex *) mutex)->owner = NULL;

        /* Enqueue current thread into the read waiting list */
        prepare_to_wait(&((struct cond *) cond)->task_wait_list,
                        &running_thread->list, THREAD_WAIT);
    }

    /* pthread_cond_wait never returns error code */
    SYSCALL_ARG(int, 0) = 0;
}

static void pthread_once_cleanup_handler(void)
{
    /* Restore the stack */
    running_thread->stack_top =
        (unsigned long *) running_thread->stack_top_preserved;
    running_thread->stack_top_preserved = (unsigned long) NULL;

    /* Wake up all waiting threads and mark once variable as complete */
    struct thread_once *once_control = running_thread->once_control;
    once_control->finished = true;
    wake_up_all(&once_control->wq);
}

void sys_pthread_once(void)
{
    typedef void (*init_routine_t)(void);

    /* Read syscall arguments */
    pthread_once_t *_once_control = SYSCALL_ARG(pthread_once_t *, 0);
    init_routine_t init_routine = SYSCALL_ARG(init_routine_t, 1);

    struct thread_once *once_control = (struct thread_once *) _once_control;

    if (once_control->finished)
        return;

    if (once_control->wq.next == NULL || once_control->wq.prev == NULL) {
        /* The first time to execute pthread_once() */
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
    /* Read syscall arguments */
    sem_t *sem = SYSCALL_ARG(sem_t *, 0);

    int retval = up((struct semaphore *) sem);
    SYSCALL_ARG(int, 0) = retval;
}

void sys_sem_trywait(void)
{
    /* Read syscall arguments */
    sem_t *sem = SYSCALL_ARG(sem_t *, 0);

    int retval = down_trylock((struct semaphore *) sem);
    SYSCALL_ARG(int, 0) = retval;
}

void sys_sem_wait(void)
{
    /* Read syscall arguments */
    sem_t *sem = SYSCALL_ARG(sem_t *, 0);

    int retval = down((struct semaphore *) sem);

    /* Check if the syscall need to be restarted */
    if (retval == -ERESTARTSYS) {
        /* Turn on the syscall pending flag */
        set_syscall_pending(running_thread);
    } else {
        /* Turn off the syscall pending flag */
        reset_syscall_pending(running_thread);

        /* Return sucesss */
        SYSCALL_ARG(int, 0) = retval;
    }
}

void sys_sem_getvalue(void)
{
    /* Read syscall arguments */
    sem_t *sem = SYSCALL_ARG(sem_t *, 0);
    int *sval = SYSCALL_ARG(int *, 0);

    *sval = ((struct semaphore *) sem)->count;

    /* Return sucesss */
    SYSCALL_ARG(int, 0) = 0;
}

void sys_sigaction(void)
{
    /* Read syscall arguments */
    int signum = SYSCALL_ARG(int, 0);
    struct sigaction *act = SYSCALL_ARG(struct sigaction *, 1);
    struct sigaction *oldact = SYSCALL_ARG(struct sigaction *, 2);

    /* Check if the signal number is defined */
    if (!is_signal_defined(signum)) {
        /* Return error */
        SYSCALL_ARG(int, 0) = -EINVAL;
        return;
    }

    /* Get the pointer of the signal action from the table */
    int sig_idx = get_signal_index(signum);
    struct sigaction *sig_entry = running_thread->sig_table[sig_idx];

    /* Has the signal action already been registered on the table? */
    if (sig_entry) {
        /* Preserve old signal action */
        if (oldact) {
            *oldact = *sig_entry;
        }

        /* Replace old signal action */
        *sig_entry = *act;
    } else {
        /* Allocate memory for new action */
        struct sigaction *new_act = kmalloc(sizeof(act));

        /* Failed to allocate memory */
        if (new_act == NULL) {
            /* Return error */
            SYSCALL_ARG(int, 0) = -ENOMEM;
            return;
        }

        /* Register signal action on the table */
        running_thread->sig_table[sig_idx] = new_act;
        *new_act = *act;
    }

    /* Return sucesss */
    SYSCALL_ARG(int, 0) = 0;
}

void sys_sigwait(void)
{
    /* Read syscall arguments */
    sigset_t *set = SYSCALL_ARG(sigset_t *, 0);
    int *sig = SYSCALL_ARG(int *, 1);

    sigset_t invalid_mask =
        ~(sig2bit(SIGUSR1) | sig2bit(SIGUSR2) | sig2bit(SIGPOLL) |
          sig2bit(SIGSTOP) | sig2bit(SIGCONT) | sig2bit(SIGKILL));

    /* Reject waiting request of an undefined signal */
    if (*set & invalid_mask) {
        /* Return error */
        SYSCALL_ARG(int, 0) = -EINVAL;
        return;
    }

    /* Record the signals to wait */
    running_thread->sig_wait_set = *set;
    running_thread->ret_sig = sig;
    running_thread->wait_for_signal = true;

    /* Enqueue the thread into the signal waiting list */
    prepare_to_wait(&suspend_list, &running_thread->list, THREAD_WAIT);
}

void sys_kill(void)
{
    /* Read syscall arguments */
    pid_t pid = SYSCALL_ARG(pid_t, 0);
    int sig = SYSCALL_ARG(int, 1);

    struct task_struct *task = acquire_task(pid);

    /* Failed to find the task */
    if (!task) {
        /* Return error */
        SYSCALL_ARG(int, 0) = -ESRCH;
        return;
    }

    /* Check if the signal number is defined */
    if (!is_signal_defined(sig)) {
        /* Return error */
        SYSCALL_ARG(int, 0) = -EINVAL;
        return;
    }

    struct list_head *curr, *next;
    list_for_each_safe (curr, next, &task->threads_list) {
        struct thread_info *thread =
            list_entry(curr, struct thread_info, task_list);
        handle_signal(thread, sig);
    }

    /* Return sucesss */
    SYSCALL_ARG(int, 0) = 0;
}

void sys_raise(void)
{
    /* Read syscall arguments */
    int sig = SYSCALL_ARG(int, 0);

    struct task_struct *task = current_task_info();

    /* Failed to find the task */
    if (!task) {
        /* Return error */
        SYSCALL_ARG(int, 0) = -ESRCH;
        return;
    }

    /* Check if the signal number is defined */
    if (!is_signal_defined(sig)) {
        /* Return error */
        SYSCALL_ARG(int, 0) = -EINVAL;
        return;
    }

    struct list_head *curr, *next;
    list_for_each_safe (curr, next, &task->threads_list) {
        struct thread_info *thread =
            list_entry(curr, struct thread_info, task_list);
        handle_signal(thread, sig);
    }

    /* Return sucesss */
    SYSCALL_ARG(int, 0) = 0;
}

void sys_clock_gettime(void)
{
    /* Read syscall arguments */
    clockid_t clockid = SYSCALL_ARG(clockid_t, 0);
    struct timespec *tp = SYSCALL_ARG(struct timespec *, 1);

    if (clockid != CLOCK_MONOTONIC) {
        /* Return error */
        SYSCALL_ARG(int, 0) = -EINVAL;
        return;
    }

    get_sys_time(tp);

    /* Return sucesss */
    SYSCALL_ARG(int, 0) = 0;
}

void sys_clock_settime(void)
{
    /* Read syscall arguments */
    clockid_t clockid = SYSCALL_ARG(clockid_t, 0);
    struct timespec *tp = SYSCALL_ARG(struct timespec *, 1);

    if (clockid != CLOCK_REALTIME) {
        /* Return error */
        SYSCALL_ARG(int, 0) = -EINVAL;
        return;
    }

    set_sys_time(tp);

    /* Return sucesss */
    SYSCALL_ARG(int, 0) = 0;
}

static struct timer *acquire_timer(int timerid)
{
    /* Find the timer with given ID */
    struct timer *timer;
    list_for_each_entry (timer, &running_thread->timers_list, list) {
        /* Compare the timer ID */
        if (timerid == timer->id)
            return timer; /* Found */
    }

    return NULL; /* Not found */
}

void sys_timer_create(void)
{
    /* Read syscall arguments */
    clockid_t clockid = SYSCALL_ARG(clockid_t, 0);
    struct sigevent *sevp = SYSCALL_ARG(struct sigevent *, 1);
    timer_t *timerid = SYSCALL_ARG(timer_t *, 2);

    /* Unsupported clock source */
    if (clockid != CLOCK_MONOTONIC) {
        /* Return error */
        SYSCALL_ARG(int, 0) = -EINVAL;
        return;
    }

    if (sevp->sigev_notify != SIGEV_NONE &&
        sevp->sigev_notify != SIGEV_SIGNAL) {
        /* Return error */
        SYSCALL_ARG(int, 0) = -EINVAL;
        return;
    }

    /* Allocate memory for the new timer */
    struct timer *new_tm = kmalloc(sizeof(struct timer));

    /* Failed to allocate memory */
    if (new_tm == NULL) {
        /* Return error */
        SYSCALL_ARG(int, 0) = -ENOMEM;
        return;
    }

    /* Record timer settings */
    new_tm->id = running_thread->timer_cnt;
    new_tm->sev = *sevp;
    new_tm->thread = running_thread;

    /* Initialize thread timer list */
    if (running_thread->timer_cnt == 0) {
        INIT_LIST_HEAD(&running_thread->timers_list);
    }

    /* Link the new timer to the list */
    list_add(&new_tm->g_list, &timers_list);
    list_add(&new_tm->list, &running_thread->timers_list);

    /* Return timer ID */
    *timerid = running_thread->timer_cnt;

    /* Increase timer count */
    running_thread->timer_cnt++;

    /* Return sucesss */
    SYSCALL_ARG(int, 0) = 0;
}

void sys_timer_delete(void)
{
    /* Read syscall arguments */
    timer_t timerid = SYSCALL_ARG(timer_t, 0);

    /* Aquire the timer with given ID */
    struct timer *timer = acquire_timer(timerid);

    /* Failed to acquire the timer */
    if (timer == NULL) {
        /* Invalid timer ID, return error */
        SYSCALL_ARG(int, 0) = -EINVAL;
        return;
    }

    /* Remove the timer from the lists and free the memory */
    list_del(&timer->g_list);
    list_del(&timer->list);
    kfree(timer);

    /* Return sucesss */
    SYSCALL_ARG(int, 0) = 0;
}

void sys_timer_settime(void)
{
    /* Read syscall arguments */
    timer_t timerid = SYSCALL_ARG(timer_t, 0);
    int flags = SYSCALL_ARG(int, 1);
    struct itimerspec *new_value = SYSCALL_ARG(struct itimerspec *, 2);
    struct itimerspec *old_value = SYSCALL_ARG(struct itimerspec *, 3);

    /* Bad arguments */
    if (new_value->it_value.tv_sec < 0 || new_value->it_value.tv_nsec < 0 ||
        new_value->it_value.tv_nsec > 999999999) {
        /* Return error */
        SYSCALL_ARG(int, 0) = -EINVAL;
        return;
    }

    /* The thread has no timer */
    if (running_thread->timer_cnt == 0) {
        /* Return error */
        SYSCALL_ARG(int, 0) = -EINVAL;
        return;
    }

    /* Acquire the timer with given ID */
    struct timer *timer = acquire_timer(timerid);

    /* Failed to acquire the timer */
    if (timer == NULL) {
        /* Return error */
        SYSCALL_ARG(int, 0) = -EINVAL;
        return;
    }

    /* Return old setting of the timer */
    if (old_value != NULL)
        *old_value = timer->setting;

    /* Save new setting of the timer */
    timer->flags = flags;
    timer->setting = *new_value;
    timer->ret_time = timer->setting;

    /* Enable the timer */
    timer->counter = timer->setting.it_value;
    timer->enabled = true;

    /* Return sucesss */
    SYSCALL_ARG(int, 0) = 0;
}

void sys_timer_gettime(void)
{
    /* Read syscall arguments */
    timer_t timerid = SYSCALL_ARG(timer_t, 0);
    struct itimerspec *curr_value = SYSCALL_ARG(struct itimerspec *, 1);

    /* Acquire the timer with given ID */
    struct timer *timer = acquire_timer(timerid);

    /* Failed to acquire the timer */
    if (timer == NULL) {
        /* invalid timer ID, return error */
        SYSCALL_ARG(int, 0) = -EINVAL;
        return;
    }

    /* Return sucesss */
    *curr_value = timer->ret_time;
    SYSCALL_ARG(int, 0) = 0;
}

void sys_malloc(void)
{
    /* Read syscall arguments */
    size_t size = SYSCALL_ARG(size_t, 0);

    void *ptr;

    /* Return NULL if the size is zero */
    if (size == 0) {
        SYSCALL_ARG(void *, 0) = NULL;
        return;
    }

    /* Allocate new memory */
    ptr = __malloc(size);

    /* Return the allocated memory */
    SYSCALL_ARG(void *, 0) = ptr;
}

void sys_free(void)
{
    /* Read syscall arguments */
    void *ptr = SYSCALL_ARG(void *, 0);

    /* Free the memory */
    __free(ptr);
}

static void threads_ticks_update(void)
{
    /* Update sleep ticks */
    struct thread_info *thread;
    list_for_each_entry (thread, &sleep_list, list) {
        /* Update remained ticks for sleeping */
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

        /* Update the timer */
        timer_down_count(&timer->counter);

        /* Update the return time */
        timer->ret_time.it_value = timer->counter;

        /* Check if the time is up */
        if (timer->counter.tv_sec != 0 || timer->counter.tv_nsec != 0) {
            continue; /* No */
        }

        /* Reload the timer */
        timer->counter = timer->setting.it_interval;

        /* Shutdown the one-shot type timer */
        if (timer->setting.it_interval.tv_sec == 0 &&
            timer->setting.it_interval.tv_nsec == 0) {
            timer->enabled = false;
        }

        /* Stage the signal handler */
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
    /* Get current time */
    struct timespec tp;
    get_sys_time(&tp);

    /* Iterate through all threads that waiting for poll events */
    struct thread_info *thread;
    list_for_each_entry (thread, &timeout_list, timeout_list) {
        /* Wake up the thread if the time is up */
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

static void thread_return_event_handler(void)
{
    struct task_struct *task = current_task_info();
    if (running_thread == task->main_thread) {
        /* Returned from the main thread, thus the whole task should
         * be terminated */
        sys_exit();
    } else {
        running_thread->retval = (void *) *running_thread->syscall_args[0];
        thread_join_handler();
    }
}

static void syscall_handler(void)
{
    unsigned long syscall_num = get_syscall_info(running_thread->stack_top,
                                                 running_thread->syscall_args);

    switch (syscall_num) {
    case 0:
        break;
    case SIGNAL_CLEANUP_EVENT:
        signal_cleanup_handler();
        break;
    case THREAD_RETURN_EVENT:
        thread_return_event_handler();
        break;
    case THREAD_ONCE_EVENT:
        pthread_once_cleanup_handler();
        break;
    default:
        /* Check syscall table */
        for (int i = 0; i < SYSCALL_CNT; i++) {
            if (syscall_num == syscall_table[i].num) {
                /* Execute the syscall service */
                syscall_table[i].syscall_handler();
                return;
            }
        }

        early_printf(
            "\r=============== SYSCALL FAULT ================\n\r"
            "Current thread: %p (%s)\n\r"
            "Faulting syscall number = %d\n\r"
            ">>> Halting system <<<\n\r"
            "==============================================",
            running_thread, running_thread->name, syscall_num);

        /* Unknown supervisor request */
        while (1)
            ;
    }
}

static void schedule(void)
{
    /* Stop current thread */
    if (running_thread->status == THREAD_RUNNING)
        prepare_to_wait(&sleep_list, &running_thread->list, THREAD_WAIT);

    /* Wake up threads that the sleep tick is exhausted */
    struct list_head *curr, *next;
    list_for_each_safe (curr, next, &sleep_list) {
        /* Acquire the thread control block */
        struct thread_info *thread = list_entry(curr, struct thread_info, list);

        /* Enqueue the thread into the ready list if it is ready */
        if (thread->sleep_ticks == 0) {
            list_move(curr, &ready_list[thread->priority]);
            thread->status = THREAD_READY;
        }
    }

    /* Find a ready list that contains runnable threads */
    int pri;
    for (pri = KTHREAD_PRI_MAX; pri >= 0; pri--) {
        if (list_empty(&ready_list[pri]) == false)
            break;
    }

    /* Select the first thread from the ready list */
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

    /* Initialize kmalloc slabs */
    for (int i = 0; i < KMALLOC_SLAB_TABLE_SIZE; i++) {
        kmalloc_caches[i] = kmem_cache_create(kmalloc_slab_info[i].name,
                                              kmalloc_slab_info[i].size,
                                              sizeof(uint32_t), 0, NULL);
    }
}

static void check_thread_stack(void)
{
    /* Calculate stack range of the thread */
    uintptr_t lower_bound = (uintptr_t) running_thread->stack;
    uintptr_t upper_bound =
        (uintptr_t) running_thread->stack + running_thread->stack_size;

    /* Check thread stack pointer is valid or not */
    if ((uintptr_t) running_thread->stack_top < lower_bound ||
        (uintptr_t) running_thread->stack_top > upper_bound) {
        early_printf(
            "\r================ STACK FAULT =================\n\r"
            "Current thread: %p (%s)\n\r"
            "Stack range: [0x%08x-0x%08x]\n\r"
            "Stack size: %d\n\r"
            "Faulting stack pointer = %p\n\r"
            ">>> Halting system <<<\n\r"
            "==============================================",
            running_thread, running_thread->name, lower_bound, upper_bound,
            running_thread->stack_size, running_thread->stack_top);

        while (1)
            ;
    }
}

void init(void)
{
    setprogname("idle");

    /* Bring up drivers */
    preempt_disable();
    {
        print_platform_info();
        __board_init();
        rom_dev_init();
    }
    preempt_enable();

    /* Mount rom file system */
    mount("/dev/rom", "/");

    /* Launched all hooked user tasks */
    extern char _tasks_start;
    extern char _tasks_end;

    int func_list_size = ((uintptr_t) &_tasks_end - (uintptr_t) &_tasks_start);
    int hook_task_cnt = func_list_size / sizeof(struct task_hook);

    struct task_hook *hook_task = (struct task_hook *) &_tasks_start;

    for (int i = 0; i < hook_task_cnt; i++) {
        task_create(hook_task[i].task_func, hook_task[i].priority,
                    hook_task[i].stacksize);
    }

    /* Run idle loop with lowest thread priority */
    while (1) {
        __idle();
    }
}

void sched_start(void)
{
    __platform_init();
    slab_init();
    heap_init();
    tty_init();
    rootfs_init();

    /* Initialize ready lists */
    for (int i = 0; i <= KTHREAD_PRI_MAX; i++) {
        INIT_LIST_HEAD(&ready_list[i]);
    }

    /* Create kernel threads for basic services */
    kthread_create(init, 0, 2048);
    kthread_create(softirqd, KTHREAD_PRI_MAX, 2048);
    kthread_create(filesysd, KTHREAD_PRI_MAX - 1, 2048);

    /* Dequeue thread 0 (Idle) to execute */
    running_thread = &threads[0];
    threads[0].status = THREAD_RUNNING;
    list_del(&threads[0].list); /* Dequeue */

    while (1) {
        if (check_systick_event(running_thread->stack_top)) {
            system_ticks_update();
        } else {
            syscall_handler();
        }

        /* Select new thread */
        schedule();

        /* Jump to the syscall handler as a thread with pending
         * syscall is selected */
        if (running_thread->syscall_pending)
            continue;

        /* Check if the selected thread has pending signals */
        check_pending_signals();

        /* Jump to the selected thread */
        running_thread->stack_top = jump_to_thread(running_thread->stack_top,
                                                   running_thread->privilege);

        /* Check thread stack pointer after it returned to the kernel */
        check_thread_stack();
    }
}
