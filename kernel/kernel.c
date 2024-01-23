#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <mpool.h>
#include <mqueue.h>
#include <poll.h>
#include <pthread.h>
#include <sched.h>
#include <semaphore.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <strings.h>
#include <sys/limits.h>
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
#include <fs/null_dev.h>
#include <fs/rom_dev.h>
#include <kernel/daemon.h>
#include <kernel/errno.h>
#include <kernel/interrupt.h>
#include <kernel/kernel.h>
#include <kernel/kfifo.h>
#include <kernel/mqueue.h>
#include <kernel/mutex.h>
#include <kernel/pipe.h>
#include <kernel/printk.h>
#include <kernel/sched.h>
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

/* Scheduler */
static bool need_resched_flag;
static uint32_t preempt_cnt;

/* Tasks and threads */
static struct task_struct tasks[TASK_MAX];

static struct thread_info threads[THREAD_MAX];
static struct thread_info *running_thread;

static uint32_t bitmap_tasks[BITMAP_SIZE(TASK_MAX)];
static uint32_t bitmap_threads[BITMAP_SIZE(THREAD_MAX)];

/* Daemons information */
static int daemon_id_table[DAEMON_CNT];

#define DECLARE_DAEMON(x) #x
static char *deamon_names[] = {DAEMON_LIST};
#undef DECLARE_DAEMON

/* Files */
struct file *files[FILE_RESERVED_NUM + FILE_MAX];
int file_cnt;

/* File descriptor table */
static struct fdtable fdtable[OPEN_MAX];
static uint32_t bitmap_fds[BITMAP_SIZE(OPEN_MAX)];

/* Message queue descriptor table */
static struct mq_desc mqd_table[MQUEUE_MAX];
static uint32_t bitmap_mqds[BITMAP_SIZE(MQUEUE_MAX)];

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

NACKED void syscall_return_handler(void)
{
    SAVE_SYSCALL_RETVAL(running_thread->syscall_args[0]);
    SYSCALL(SYSCALL_RETURN_EVENT);
}

NACKED void thread_return_handler(void)
{
    SYSCALL(THREAD_RETURN_EVENT);
}

NACKED void signal_cleanup_handler(void)
{
    SYSCALL(SIGNAL_CLEANUP_EVENT);
}

NACKED void thread_once_return_handler(void)
{
    SYSCALL(THREAD_ONCE_EVENT);
}

inline void preempt_count_inc(void)
{
    preempt_cnt++;
}

inline void preempt_count_dec(void)
{
    preempt_cnt--;
}

void preempt_disable(void)
{
    if (preempt_cnt == 0)
        __preempt_disable();

    /* Increase nesting level */
    preempt_count_inc();
}

void preempt_enable(void)
{
    /* Decrease nesting level */
    preempt_count_dec();

    if (preempt_cnt == 0)
        __preempt_enable();
}

static inline int preempt_count(void)
{
    return preempt_cnt;
}

void *kmalloc(size_t size)
{
    /* Start the critcal section */
    preempt_disable();

    void *retval = NULL, *ptr = NULL;

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
            printk("kmalloc(): failed as the request size %d is too large",
                   size);
        }
    }

    if (ptr) {
        /* Record the allocated size and return the start address */
        ((struct kmalloc_header *) ptr)->size = size;
        retval = (void *) ((uintptr_t) ptr + header_size);
    }

    /* End the critical section */
    preempt_enable();

    return retval;
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

static inline struct task_struct *current_task_info(void)
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
        halt();
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
    size_t buf_size = ALIGN(sizeof(char) * PIPE_BUF, sizeof(long));

    struct pipe *pipe = (struct pipe *) ((uintptr_t) stack_top - pipe_size);
    struct kfifo *pipe_fifo = (struct kfifo *) ((uintptr_t) pipe - kfifo_size);
    char *buf = (char *) ((uintptr_t) pipe_fifo - buf_size);

    kfifo_init(pipe_fifo, buf, sizeof(char), PIPE_BUF);
    pipe->fifo = pipe_fifo;
    fifo_init(THREAD_PIPE_FD(tid), (struct file **) &files, NULL, pipe);

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
                         bool kernel_thread)
{
    /* Check if the detach state setting is invalid */
    bool bad_detach_state = attr->detachstate != PTHREAD_CREATE_DETACHED &&
                            attr->detachstate != PTHREAD_CREATE_JOINABLE;

    /* Check if the thread priority is invalid */
    bool bad_priority;
    if (kernel_thread) {
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
    int tid = find_first_zero_bit(bitmap_threads, THREAD_MAX);
    if (tid >= THREAD_MAX)
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
    thread->kernel_thread = kernel_thread;
    thread->privilege = kernel_thread ? KERNEL_THREAD : USER_THREAD;

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
                        bool kernel_thread)
{
    struct thread_attr attr = {
        .schedparam.sched_priority = priority,
        .stackaddr = NULL,
        .stacksize = stack_size,
        .schedpolicy = SCHED_RR,
        .detachstate = PTHREAD_CREATE_JOINABLE,
    };

    struct thread_info *thread;
    int retval = thread_create(&thread, task_func, &attr, NULL, kernel_thread);
    if (retval != 0)
        return retval;

    /* Allocate new task ID */
    int pid = find_first_zero_bit(bitmap_tasks, TASK_MAX);
    if (pid >= TASK_MAX)
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
    preempt_disable();

    int retval = _task_create(task_func, priority, stack_size, true);
    if (retval < 0)
        printk("kthread_create(): failed to create new task");

    preempt_enable();

    return retval;
}

static void task_delete(struct task_struct *task)
{
    list_del(&task->list);
    bitmap_clear_bit(bitmap_tasks, task->pid);

    for (int i = 0; i < BITMAP_SIZE(OPEN_MAX); i++) {
        bitmap_fds[i] &= ~task->bitmap_fds[i];
    }

    for (int i = 0; i < BITMAP_SIZE(MQUEUE_MAX); i++) {
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
                            (uint32_t) signal_cleanup_handler, info.args);

    /* Update the number of total pending signals */
    running_thread->signal_cnt = kfifo_len(&running_thread->signal_queue);
}

static void thread_suspend(struct thread_info *thread)
{
    if (thread->status == THREAD_SUSPENDED)
        return;

    prepare_to_wait(&suspend_list, thread, THREAD_SUSPENDED);
}

static void thread_resume(struct thread_info *thread)
{
    if (thread->status != THREAD_SUSPENDED)
        return;

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
                     struct thread_info *thread,
                     int state)
{
    preempt_disable();

    list_add(&thread->list, wait_list);
    thread->status = state;

    preempt_enable();
}

void finish_wait(struct thread_info *thread)
{
    preempt_disable();

    if (thread != running_thread) {
        thread->status = THREAD_READY;
        list_move(&thread->list, &ready_list[thread->priority]);
    }

    preempt_enable();
}

void wake_up(struct list_head *wait_list)
{
    preempt_disable();

    if (!list_empty(wait_list)) {
        /* Wake up the first thread in the waiting list */
        struct thread_info *thread =
            list_first_entry(wait_list, struct thread_info, list);
        list_move(&thread->list, &ready_list[thread->priority]);
        thread->status = THREAD_READY;
    }

    preempt_enable();
}

void wake_up_all(struct list_head *wait_list)
{
    preempt_disable();

    /* Wake up all threads from the waiting list */
    struct list_head *curr, *next;
    list_for_each_safe (curr, next, wait_list) {
        struct thread_info *thread = list_entry(curr, struct thread_info, list);

        list_move(&thread->list, &ready_list[thread->priority]);
        thread->status = THREAD_READY;
    }

    preempt_enable();
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

        finish_wait(thread);
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
    for (int i = tid + 1; i < THREAD_MAX; i++) {
        if (bitmap_get_bit(bitmap_threads, i)) {
            thread = &threads[i];
            break;
        }
    }

    return thread;
}

static void *sys_thread_info(struct thread_stat *info, void *next)
{
    preempt_disable();

    void *retval;

    int tid;
    struct thread_info *thread = NULL;

    if (next == NULL) {
        /* Assign the first thread */
        tid = find_first_bit(bitmap_threads, THREAD_MAX);
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
                retval = NULL;
                goto leave;
            }
        }
    }

    /* Return thread information */
    info->pid = thread->task->pid;
    info->tid = thread->tid;
    info->priority = thread->priority;
    info->kernel_thread = thread->kernel_thread;
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
    retval = thread_info_find_next(thread);

leave:
    preempt_enable();
    return retval;
}

static void sys_setprogname(const char *name)
{
    preempt_disable();
    strncpy(running_thread->name, name, THREAD_NAME_MAX);
    preempt_enable();
}

static int sys_delay_ticks(uint32_t ticks)
{
    preempt_disable();

    /* Reconfigure the tick to sleep */
    running_thread->sleep_ticks = ticks;

    /* Enqueue the thread into the sleep list */
    running_thread->status = THREAD_WAIT;
    list_add(&(running_thread->list), &sleep_list);

    preempt_enable();

    /* Return success */
    return 0;
}

static int sys_task_create(task_func_t task_func,
                           uint8_t priority,
                           int stack_size)
{
    preempt_disable();

    int retval = _task_create(task_func, priority, stack_size, false);
    if (retval < 0)
        printk("task_create(): failed to create new task");

    preempt_enable();

    /* Return task creation result */
    return retval;
}

static void *sys_mpool_alloc(struct mpool *mpool, size_t size)
{
    preempt_disable();

    void *ptr = NULL;
    size_t alloc_size = ALIGN(size, sizeof(long));

    /* Check if the memory poll has enough space */
    if ((mpool->offset + alloc_size) <= mpool->size) {
        ptr = (void *) ((uintptr_t) mpool->mem + mpool->offset);
        mpool->offset += alloc_size;
    }

    preempt_enable();

    /* Return the allocated memory */
    return ptr;
}

static int sys_minfo(int name)
{
    preempt_disable();

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

    preempt_enable();

    /* Return the inquired information */
    return retval;
}

static int sys_sched_yield(void)
{
    /* Suspend current thread */
    prepare_to_wait(&sleep_list, running_thread, THREAD_WAIT);

    /* Return success */
    return 0;
}

static void sys_exit(int status)
{
    preempt_disable();

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

    preempt_enable();
}

static int sys_mount(const char *source, const char *target)
{
    /* Check the length of the pathname */
    if (strlen(source) >= PATH_MAX || strlen(target) >= PATH_MAX)
        return -ENAMETOOLONG;

    int tid = running_thread->tid;

    /* Send mount request to the file system daemon */
    request_mount(tid, source, target);

    /* Read mount result from the file system daemon */
    int fifo_retval, mnt_result;
    while (1) {
        fifo_retval = fifo_read(files[THREAD_PIPE_FD(tid)],
                                (char *) &mnt_result, sizeof(mnt_result), 0);

        if (fifo_retval != -ERESTARTSYS)
            break;

        schedule();
    }

    return mnt_result;
}

static int sys_open(const char *pathname, int flags)
{
    int retval;

    preempt_disable();

    /* Check the length of the pathname */
    if (strlen(pathname) >= PATH_MAX) {
        retval = -ENAMETOOLONG;
        goto err;
    }

    /* Acquire the running task */
    struct task_struct *task = current_task_info();

    /* Acquire the thread ID */
    int tid = running_thread->tid;

    preempt_enable();

    /* Send file open request to the file system daemon */
    request_open_file(tid, pathname);

    /* Read the file index from the file system daemon */
    int file_idx;
    int fifo_retval;
    while (1) {
        fifo_retval = fifo_read(files[THREAD_PIPE_FD(tid)], (char *) &file_idx,
                                sizeof(file_idx), 0);

        if (fifo_retval != -ERESTARTSYS)
            break;

        schedule();
    }

    preempt_disable();

    /* File not found */
    if (file_idx == -1) {
        /* Return error */
        retval = -ENOENT;
        goto err;
    }

    /* Find a free entry on the file descriptor entry table */
    int fdesc_idx = find_first_zero_bit(bitmap_fds, OPEN_MAX);
    if (fdesc_idx >= OPEN_MAX) {
        /* Return error */
        retval = -ENOMEM;
        goto err;
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
        retval = -ENXIO;
        goto err;
    }

    preempt_enable();

    /* Call open operation  */
    filp->f_op->open(filp->f_inode, filp);

    /* Return the file descriptor number */
    int fd = fdesc_idx + FILE_RESERVED_NUM;
    return fd;

err:
    preempt_enable();
    return retval;
}

static int sys_close(int fd)
{
    preempt_disable();

    int retval;

    /* Acquire the running task */
    struct task_struct *task = current_task_info();

    /* Invalid file descriptor number */
    if (fd < FILE_RESERVED_NUM) {
        retval = -EBADF;
        goto leave;
    }

    /* Calculate the index number of the file descriptor */
    int fdesc_idx = fd - FILE_RESERVED_NUM;

    /* Check if the file descriptor belongs to current task */
    if (!bitmap_get_bit(bitmap_fds, fdesc_idx) ||
        !bitmap_get_bit(task->bitmap_fds, fdesc_idx)) {
        retval = -EBADF;
        goto leave;
    }

    /* Free the file descriptor */
    bitmap_clear_bit(bitmap_fds, fdesc_idx);
    bitmap_clear_bit(task->bitmap_fds, fdesc_idx);

    /* Return success */
    retval = 0;

leave:
    preempt_enable();
    return retval;
}

static int sys_dup(int oldfd)
{
    preempt_disable();

    int retval;

    if (oldfd < FILE_RESERVED_NUM) {
        retval = -EBADF;
        goto leave;
    }

    /* Convert oldfd to the index numbers on the table */
    int old_fdesc_idx = oldfd - FILE_RESERVED_NUM;

    /* Acquire the running task */
    struct task_struct *task = current_task_info();

    /* Check if the file descriptor is invalid */
    if (!bitmap_get_bit(bitmap_fds, old_fdesc_idx) ||
        !bitmap_get_bit(task->bitmap_fds, old_fdesc_idx)) {
        retval = -EBADF;
        goto leave;
    }

    /* Find a free entry on the file descriptor table */
    int fdesc_idx = find_first_zero_bit(bitmap_fds, OPEN_MAX);
    if (fdesc_idx >= OPEN_MAX) {
        retval = -ENOMEM;
        goto leave;
    }
    bitmap_set_bit(bitmap_fds, fdesc_idx);
    bitmap_set_bit(task->bitmap_fds, fdesc_idx);

    /* Copy the old file descriptor content to the new one */
    fdtable[fdesc_idx] = fdtable[old_fdesc_idx];

    /* Return new file descriptor number */
    retval = fdesc_idx + FILE_RESERVED_NUM;

leave:
    preempt_enable();
    return retval;
}

static int sys_dup2(int oldfd, int newfd)
{
    preempt_disable();

    int retval;

    /* Convert fds to the index numbers on the table */
    int old_fdesc_idx = oldfd - FILE_RESERVED_NUM;
    int new_fdesc_idx = newfd - FILE_RESERVED_NUM;

    if (oldfd < FILE_RESERVED_NUM || newfd < FILE_RESERVED_NUM) {
        retval = -EBADF;
        goto leave;
    }

    /* Acquire the running task */
    struct task_struct *task = current_task_info();

    /* Check if the file descriptors are invalid */
    if (!bitmap_get_bit(bitmap_fds, old_fdesc_idx) ||
        !bitmap_get_bit(bitmap_fds, new_fdesc_idx) ||
        !bitmap_get_bit(task->bitmap_fds, old_fdesc_idx) ||
        !bitmap_get_bit(task->bitmap_fds, new_fdesc_idx)) {
        retval = -EBADF;
        goto leave;
    }

    /* Copy the old file descriptor content to the new one */
    fdtable[new_fdesc_idx] = fdtable[old_fdesc_idx];

    /* Return new file descriptor */
    retval = newfd;

leave:
    preempt_enable();
    return retval;
}

static ssize_t sys_read(int fd, void *buf, size_t count)
{
    ssize_t retval;

    preempt_disable();

    /* Acquire the running task */
    struct task_struct *task = current_task_info();

    /* Get the file to read */
    struct file *filp;
    if (fd < FILE_RESERVED_NUM) {
        /* Read target is the anonymous pipe of a thread */
        filp = files[fd];
    } else {
        /* Calculate the index number of the file descriptor
         * on the table */
        int fdesc_idx = fd - FILE_RESERVED_NUM;

        /* Check if the file descriptor is invalid */
        if (!bitmap_get_bit(bitmap_fds, fdesc_idx) ||
            !bitmap_get_bit(task->bitmap_fds, fdesc_idx)) {
            retval = -EBADF;
            goto err;
        }

        filp = fdtable[fdesc_idx].file;
        filp->f_flags = fdtable[fdesc_idx].flags;
    }

    /* Check if the file operation is undefined */
    if (!filp->f_op->read) {
        /* Return error */
        retval = -ENXIO;
        goto err;
    }

    preempt_enable();

    /* Call read operation */
    while (1) {
        retval = filp->f_op->read(filp, buf, count, 0);

        if (retval != -ERESTARTSYS)
            break;

        schedule();
    }

    return retval;

err:
    preempt_enable();
    return retval;
}

static ssize_t sys_write(int fd, const void *buf, size_t count)
{
    ssize_t retval;

    preempt_disable();

    /* Acquire the running task */
    struct task_struct *task = current_task_info();

    /* Get the file pointer */
    struct file *filp;
    if (fd < FILE_RESERVED_NUM) {
        /* Write target is the anonymous pipe of a thread */
        filp = files[fd];
    } else {
        /* Calculate the index number of the file descriptor
         * on the table */
        int fdesc_idx = fd - FILE_RESERVED_NUM;

        /* Check if the file descriptor is invalid */
        if (!bitmap_get_bit(bitmap_fds, fdesc_idx) ||
            !bitmap_get_bit(task->bitmap_fds, fdesc_idx)) {
            retval = -EBADF;
            goto err;
        }

        filp = fdtable[fdesc_idx].file;
        filp->f_flags = fdtable[fdesc_idx].flags;
    }

    /* Check if the file operation is undefined */
    if (!filp->f_op->write) {
        /* Return error */
        retval = -ENXIO;
        goto err;
    }

    preempt_enable();

    /* Call write operation */
    while (1) {
        retval = filp->f_op->write(filp, buf, count, 0);

        if (retval != -ERESTARTSYS)
            break;

        schedule();
    };

    return retval;

err:
    preempt_enable();
    return retval;
}

static int sys_ioctl(int fd, unsigned int request, unsigned long arg)
{
    int retval;

    preempt_disable();

    /* Acquire the running task */
    struct task_struct *task = current_task_info();

    /* Get the file pointer */
    struct file *filp;
    if (fd < FILE_RESERVED_NUM) {
        /* I/O control target is the anonymous pipe of a thread */
        filp = files[fd];
    } else {
        /* calculate the index number of the file descriptor
         * on the table */
        int fdesc_idx = fd - FILE_RESERVED_NUM;

        /* Check if the file descriptor is invalid */
        if (!bitmap_get_bit(bitmap_fds, fdesc_idx) ||
            !bitmap_get_bit(task->bitmap_fds, fdesc_idx)) {
            retval = -EBADF;
            goto err;
        }

        filp = fdtable[fdesc_idx].file;
    }

    /* Check if the file operation is undefined */
    if (!filp->f_op->ioctl) {
        /* Return error */
        retval = -ENXIO;
        goto err;
    }

    preempt_enable();

    /* Call ioctl operation */
    while (1) {
        retval = filp->f_op->ioctl(filp, request, arg);

        if (retval != -ERESTARTSYS)
            break;

        schedule();
    }

    return retval;

err:
    preempt_enable();
    return retval;
}

static off_t sys_lseek(int fd, long offset, int whence)
{
    off_t retval;

    preempt_disable();

    /* Acquire the running task */
    struct task_struct *task = current_task_info();

    /* Get the file pointer */
    struct file *filp;
    if (fd < FILE_RESERVED_NUM) {
        /* lseek target is the anonymous pipe of a thread */
        filp = files[fd];
    } else {
        /* Calculate the index number of the file descriptor
         * on the table */
        int fdesc_idx = fd - FILE_RESERVED_NUM;

        /* Check if the file descriptor is invalid */
        if (!bitmap_get_bit(bitmap_fds, fdesc_idx) ||
            !bitmap_get_bit(task->bitmap_fds, fdesc_idx)) {
            retval = -EBADF;
            goto err;
        }

        filp = fdtable[fdesc_idx].file;
    }

    /* Check if the file operation is undefined */
    if (!filp->f_op->lseek) {
        /* Return error */
        retval = -ENXIO;
        goto err;
    }

    preempt_enable();

    /* Call lseek operation */
    while (1) {
        retval = filp->f_op->lseek(filp, offset, whence);

        if (retval != -ERESTARTSYS)
            break;

        schedule();
    }

    return retval;

err:
    preempt_enable();
    return retval;
}

static int sys_fstat(int fd, struct stat *statbuf)
{
    int retval;

    preempt_disable();

    /* Acquire the running task */
    struct task_struct *task = current_task_info();

    if (fd < FILE_RESERVED_NUM) {
        retval = -EBADF;
        goto leave;
    }

    /* Calculate the index number of the file descriptor
     * on the table */
    int fdesc_idx = fd - FILE_RESERVED_NUM;

    /* Check if the file descriptor is invalid */
    if (!bitmap_get_bit(bitmap_fds, fdesc_idx) ||
        !bitmap_get_bit(task->bitmap_fds, fdesc_idx)) {
        retval = -EBADF;
        goto leave;
    }

    /* Get file inode */
    struct inode *inode = fdtable[fdesc_idx].file->f_inode;

    /* Check if the inode exists */
    if (inode != NULL) { /* XXX */
        statbuf->st_mode = inode->i_mode;
        statbuf->st_ino = inode->i_ino;
        statbuf->st_rdev = inode->i_rdev;
        statbuf->st_size = inode->i_size;
        statbuf->st_blocks = inode->i_blocks;
    }

    /* Return success */
    retval = 0;

leave:
    preempt_enable();
    return retval;
}

static int sys_opendir(const char *pathname, DIR *dirp /* FIXME */)
{
    /* Check the length of the pathname */
    if (strlen(pathname) >= PATH_MAX)
        return -ENAMETOOLONG;

    int tid = running_thread->tid;

    /* Send directory open request to the file system daemon */
    request_open_directory(tid, pathname);

    /* Read the directory inode from the file system daemon */
    struct inode *inode_dir;
    int fifo_retval;
    while (1) {
        fifo_retval = fifo_read(files[THREAD_PIPE_FD(tid)], (char *) &inode_dir,
                                sizeof(inode_dir), 0);

        if (fifo_retval != -ERESTARTSYS)
            break;

        schedule();
    }

    /* Return directory information */
    dirp->inode_dir = inode_dir;
    dirp->dentry_list = inode_dir->i_dentry.next;

    /* Check if the information is retrieved successfully */
    return dirp->inode_dir ? 0 : -ENOENT;
}

static int sys_readdir(DIR *dirp, struct dirent *dirent)
{
    preempt_disable();
    int retval = fs_read_dir(dirp, dirent);
    preempt_enable();

    return retval;
}

static char *sys_getcwd(char *buf, size_t size)
{
    int tid = running_thread->tid;

    /* Send getcwd request to the file system daemon */
    request_getcwd(tid, buf, size);

    /* Read getcwd result from the file system daemon */
    char *path;
    int fifo_retval;
    while (1) {
        fifo_retval = fifo_read(files[THREAD_PIPE_FD(tid)], (char *) &path,
                                sizeof(path), 0);

        if (fifo_retval != -ERESTARTSYS)
            break;

        schedule();
    }

    return path;
}

static int sys_chdir(const char *path)
{
    int tid = running_thread->tid;

    /* Send chdir request to the file system daemon */
    request_chdir(tid, path);

    /* Read chdir result from the file system daemon */
    int chdir_result;
    int fifo_retval;
    while (1) {
        fifo_retval =
            fifo_read(files[THREAD_PIPE_FD(tid)], (char *) &chdir_result,
                      sizeof(chdir_result), 0);

        if (fifo_retval != -ERESTARTSYS)
            break;

        schedule();
    }

    return chdir_result;
}

static int sys_getpid(void)
{
    return current_task_info()->pid;
}

static int sys_mknod(const char *pathname, mode_t mode, dev_t dev)
{
    /* Check the length of the pathname */
    if (strlen(pathname) >= PATH_MAX)
        return -ENAMETOOLONG;

    int tid = running_thread->tid;

    /* Send file create request to the file system daemon */
    request_create_file(tid, pathname, dev);

    /* Read file index from the file system daemon  */
    int file_idx;
    int fifo_retval;
    while (1) {
        fifo_retval = fifo_read(files[THREAD_PIPE_FD(tid)], (char *) &file_idx,
                                sizeof(file_idx), 0);

        if (fifo_retval != -ERESTARTSYS)
            break;

        schedule();
    }

    if (file_idx == -1) {
        /* Failed to create file */
        return -1; /* TODO: Specify the failed reason */
    } else {
        /* Return success */
        return 0;
    }
}

static int sys_mkfifo(const char *pathname, mode_t mode)
{
    /* Check the length of the pathname */
    if (strlen(pathname) >= PATH_MAX)
        return -ENAMETOOLONG;

    int tid = running_thread->tid;

    /* Send file create request to the file system daemon */
    request_create_file(tid, pathname, S_IFIFO);

    /* Read the file index from the file system daemon */
    int file_idx = 0;
    int fifo_retval;
    while (1) {
        fifo_retval = fifo_read(files[THREAD_PIPE_FD(tid)], (char *) &tid,
                                sizeof(file_idx), 0);

        if (fifo_retval != -ERESTARTSYS)
            break;

        schedule();
    }

    if (file_idx == -1) {
        /* Failed to create FIFO */
        return -1; /* TODO: Specify the failed reason */
    } else {
        /* Return success */
        return 0;
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
                finish_wait(thread);
        }
    }
}

static int sys_poll(struct pollfd *fds, nfds_t nfds, int timeout)
{
    int retval;

    preempt_disable();

    /* Set polling deadline */
    if (timeout > 0) {
        struct timespec tp;
        get_sys_time(&tp);
        time_add(&tp, 0, timeout * 1000000);
        running_thread->syscall_timeout = tp;
    }

    /* Initialize the polling file list */
    INIT_LIST_HEAD(&running_thread->poll_files_list);

    /* Check file events */
    struct file *filp;
    bool no_event = true;

    for (int i = 0; i < nfds; i++) {
        int fd = fds[i].fd - FILE_RESERVED_NUM;
        filp = fdtable[fd].file;

        uint32_t events = filp->f_events;
        if (~fds[i].events & events) {
            no_event = false;
        }

        /* Return file events */
        fds[i].revents |= events;
    }

    if (!no_event) {
        /* Return success */
        retval = 0;
        goto leave;
    }

    /* No events is observed and no timeout is set, return immediately */
    if (timeout == 0) {
        /* return error */
        retval = -1; /* TODO: Specify the failed reason */
        goto leave;
    }

    /* Suspend current thread */
    prepare_to_wait(&poll_list, running_thread, THREAD_WAIT);

    /* Add current thread into the timeout monitoring list */
    if (timeout > 0)
        list_add(&running_thread->timeout_list, &timeout_list);

    /* Record all files for polling */
    for (int i = 0; i < nfds; i++) {
        int fd = fds[i].fd - FILE_RESERVED_NUM;
        filp = fdtable[fd].file;
        list_add(&filp->list, &running_thread->poll_files_list);
    }

    preempt_enable();

    /* Wait until the file event happens */
    schedule();

    preempt_disable();

    /* clear list of poll files */
    INIT_LIST_HEAD(&running_thread->poll_files_list);

    /* Remove the thread from the polling list */
    if (timeout > 0)
        list_del(&running_thread->timeout_list);

    /* TODO: Specify the failed reason */
    retval = (running_thread->syscall_is_timeout) ? -1 : 0;

leave:
    preempt_enable();
    return retval;
}

static int sys_mq_getattr(mqd_t mqdes, struct mq_attr *attr)
{
    preempt_disable();

    int retval;

    /* Check if the message queue descriptor is invalid */
    struct task_struct *task = current_task_info();
    if (!bitmap_get_bit(bitmap_mqds, mqdes) ||
        !bitmap_get_bit(task->bitmap_mqds, mqdes)) {
        retval = -EBADF;
        goto leave;
    }

    /* Return attributes */
    attr->mq_flags = mqd_table[mqdes].attr.mq_flags;
    attr->mq_maxmsg = mqd_table[mqdes].attr.mq_maxmsg;
    attr->mq_msgsize = mqd_table[mqdes].attr.mq_msgsize;
    attr->mq_curmsgs = __mq_len(mqd_table[mqdes].mq);

    /* Return success */
    retval = 0;

leave:
    preempt_enable();
    return retval;
}

static int sys_mq_setattr(mqd_t mqdes,
                          const struct mq_attr *newattr,
                          struct mq_attr *oldattr)
{
    preempt_disable();

    int retval;

    struct task_struct *task = current_task_info();
    if (!bitmap_get_bit(bitmap_mqds, mqdes) ||
        !bitmap_get_bit(task->bitmap_mqds, mqdes)) {
        retval = -EBADF;
        goto leave;
    }

    /* Acquire message queue attributes from the table */
    struct mq_attr *curr_attr = &mqd_table[mqdes].attr;

    /* Only the O_NONBLOCK bit-field in mq_flags can be changed */
    if (newattr->mq_flags & ~O_NONBLOCK ||
        newattr->mq_maxmsg != curr_attr->mq_maxmsg ||
        newattr->mq_msgsize != curr_attr->mq_msgsize) {
        /* Return error */
        retval = -EINVAL;
        goto leave;
    }

    /* Preserve old attributes */
    oldattr->mq_flags = curr_attr->mq_flags;
    oldattr->mq_maxmsg = curr_attr->mq_maxmsg;
    oldattr->mq_msgsize = curr_attr->mq_msgsize;
    oldattr->mq_curmsgs = __mq_len(mqd_table[mqdes].mq);

    /* Save new mq_flags attribute */
    curr_attr->mq_flags = (~O_NONBLOCK) & newattr->mq_flags;

    /* Return success */
    retval = 0;

leave:
    preempt_enable();
    return retval;
}

struct mqueue *acquire_mqueue(const char *name)
{
    /* Find the message queue with the given name */
    struct mqueue *mq;
    list_for_each_entry (mq, &mqueue_list, list) {
        if (!strncmp(name, mq->name, NAME_MAX))
            return mq; /* Found */
    }

    /* Not found */
    return NULL;
}

static mqd_t sys_mq_open(const char *name, int oflag, struct mq_attr *attr)
{
    preempt_disable();

    mqd_t retval;

    /* Acquire the running task */
    struct task_struct *task = current_task_info();

    /* Search the message with the given name */
    struct mqueue *mq = acquire_mqueue(name);

    /* Check if new message queue descriptor can be dispatched */
    int mqdes = find_first_zero_bit(bitmap_mqds, MQUEUE_MAX);
    if (mqdes >= MQUEUE_MAX) {
        retval = -ENOMEM;
        goto leave;
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
            retval = -EEXIST;
            goto leave;
        }

        /* Register new message queue descriptor */
        bitmap_set_bit(bitmap_mqds, mqdes);
        bitmap_set_bit(task->bitmap_mqds, mqdes);
        mqd_table[mqdes].mq = mq;
        mqd_table[mqdes].attr = *attr;
        mqd_table[mqdes].attr.mq_flags = oflag;

        /* Return message queue descriptor */
        retval = mqdes;
        goto leave;
    }

    /* O_CREAT flag is not set */
    if (!(oflag & O_CREAT)) {
        /* Return error */
        retval = -ENOENT;
        goto leave;
    }

    /* Allocate new message queue */
    struct mqueue *new_mq = __mq_allocate(attr);

    /* Memory allocation failure */
    if (!new_mq) {
        /* Return error */
        retval = -ENOMEM;
        goto leave;
    }

    /* Set up the message queue */
    strncpy(new_mq->name, name, NAME_MAX);
    list_add(&new_mq->list, &mqueue_list);

    /* Register new message queue descriptor */
    bitmap_set_bit(bitmap_mqds, mqdes);
    bitmap_set_bit(task->bitmap_mqds, mqdes);
    mqd_table[mqdes].mq = new_mq;
    mqd_table[mqdes].attr = *attr;
    mqd_table[mqdes].attr.mq_flags = oflag;

    /* Return the message queue descriptor */
    retval = mqdes;

leave:
    preempt_enable();
    return retval;
}

static int sys_mq_close(mqd_t mqdes)
{
    preempt_disable();

    int retval;

    /* Check if the message queue descriptor is invalid */
    struct task_struct *task = current_task_info();
    if (!bitmap_get_bit(bitmap_mqds, mqdes) ||
        !bitmap_get_bit(task->bitmap_mqds, mqdes)) {
        retval = -EBADF;
        goto leave;
    }

    /* Free the message queue descriptor */
    bitmap_clear_bit(bitmap_mqds, mqdes);
    bitmap_clear_bit(task->bitmap_mqds, mqdes);

    /* Return success */
    retval = 0;

leave:
    preempt_enable();
    return retval;
}

static int sys_mq_unlink(const char *name)
{
    preempt_disable();

    int retval;

    /* Check the length of the message queue name */
    if (strlen(name) >= NAME_MAX) {
        retval = -ENAMETOOLONG;
        goto leave;
    }

    /* Search the message with the given name */
    struct mqueue *mq = acquire_mqueue(name);

    /* Check if the message queue exists */
    if (!mq) {
        retval = -ENOENT;
        goto leave;
    }

    /* Remove the message queue from the system */
    __mq_free(mq);

    /* Return success */
    retval = 0;

leave:
    preempt_enable();
    return retval;
}

static ssize_t sys_mq_receive(mqd_t mqdes,
                              char *msg_ptr,
                              size_t msg_len,
                              unsigned int *msg_prio)
{
    ssize_t retval;

    preempt_disable();

    /* Check if the message queue descriptor is invalid */
    struct task_struct *task = current_task_info();
    if (!bitmap_get_bit(bitmap_mqds, mqdes) ||
        !bitmap_get_bit(task->bitmap_mqds, mqdes)) {
        retval = -EBADF;
        goto err;
    }

    /* Acquire the message queue */
    struct mqueue *mq = mqd_table[mqdes].mq;

    /* Check if msg_len exceeds maximum size */
    if (msg_len > mqd_table[mqdes].attr.mq_msgsize) {
        retval = -EMSGSIZE;
        goto err;
    }

    preempt_enable();

    /* Read message */
    while (1) {
        retval = __mq_receive(mq, &mqd_table[mqdes].attr, msg_ptr, msg_len,
                              msg_prio);

        if (retval != -ERESTARTSYS)
            break;

        schedule();
    }

    return retval;

err:
    preempt_enable();
    return retval;
}

static int sys_mq_send(mqd_t mqdes,
                       const char *msg_ptr,
                       size_t msg_len,
                       unsigned int msg_prio)
{
    int retval;

    preempt_disable();

    /* Check if the message priority exceeds the max value */
    if (msg_prio > MQ_PRIO_MAX) {
        retval = -EINVAL;
        goto err;
    }

    /* Check if the message queue descriptor is invalid */
    struct task_struct *task = current_task_info();
    if (!bitmap_get_bit(bitmap_mqds, mqdes) ||
        !bitmap_get_bit(task->bitmap_mqds, mqdes)) {
        retval = -EBADF;
        goto err;
    }

    /* Acquire the message queue */
    struct mqueue *mq = mqd_table[mqdes].mq;

    /* Check if msg_len exceeds maximum size */
    if (msg_len > mqd_table[mqdes].attr.mq_msgsize) {
        retval = -EMSGSIZE;
        goto err;
    }

    preempt_enable();

    /* Send message */
    while (1) {
        retval =
            __mq_send(mq, &mqd_table[mqdes].attr, msg_ptr, msg_len, msg_prio);

        if (retval != -ERESTARTSYS)
            break;

        schedule();
    }

    return retval;

err:
    preempt_enable();
    return retval;
}

static int sys_pthread_create(pthread_t *pthread,
                              const pthread_attr_t *_attr,
                              void *(*start_routine)(void *),
                              void *arg)
{
    preempt_disable();

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
                               arg, running_thread->kernel_thread);

    /* Thread is created sucessfully */
    if (retval == 0) {
        strncpy(thread->name, running_thread->name, THREAD_NAME_MAX);

        /* Set task ownership to the thread */
        thread->task = current_task_info();
        list_add(&thread->task_list, &current_task_info()->threads_list);

        /* Return thread ID */
        *pthread = thread->tid;
    }

    preempt_enable();

    return retval;
}

static pthread_t sys_pthread_self(void)
{
    return running_thread->tid;
}

static int sys_pthread_join(pthread_t tid, void **pthread_retval)
{
    preempt_disable();

    int retval;

    struct thread_info *thread = acquire_thread(tid);
    if (!thread) {
        /* Return error */
        retval = -ESRCH;
        goto leave;
    }

    if (thread->detached || !thread->joinable) {
        /* Return error */
        retval = -EINVAL;
        goto leave;
    }

    /* Check deadlock (threads should not join on each other) */
    struct thread_info *check_thread;
    list_for_each_entry (check_thread, &running_thread->join_list, list) {
        /* Check if the thread is waiting to join the running thread */
        if (check_thread == thread) {
            /* Deadlock identified, return error */
            retval = -EDEADLK;
            goto leave;
        }
    }

    running_thread->retval_join = pthread_retval;

    list_add(&running_thread->list, &thread->join_list);
    running_thread->status = THREAD_WAIT;

    /* Return success */
    retval = 0;

leave:
    preempt_enable();
    return retval;
}

static int sys_pthread_cancel(pthread_t tid)
{
    preempt_disable();

    int retval;

    struct thread_info *thread = acquire_thread(tid);
    if (!thread) {
        /* Return error */
        retval = -ESRCH;
        goto leave;
    }

    /* kthread can only be canceled by the kernel */
    if (thread->privilege == KERNEL_THREAD) {
        /* Return error */
        retval = -EPERM;
        goto leave;
    }

    thread_delete(thread);

    /* Return success */
    retval = 0;

leave:
    preempt_enable();
    return retval;
}

static int sys_pthread_detach(pthread_t tid)
{
    preempt_disable();

    int retval;

    /* Check if the thread exists */
    if (!bitmap_get_bit(bitmap_threads, tid)) {
        /* Return error */
        retval = -ESRCH;
        goto leave;
    }

    threads[tid].detached = true;

    /* Return success */
    retval = 0;

leave:
    preempt_enable();
    return retval;
}

static int sys_pthread_setschedparam(pthread_t tid,
                                     int policy,
                                     const struct sched_param *param)
{
    preempt_disable();

    int retval;

    struct thread_info *thread = acquire_thread(tid);
    if (!thread) {
        /* Return error */
        retval = -ESRCH;
        goto leave;
    }

    /* kthread can only be configured by the kernel */
    if (thread->privilege == KERNEL_THREAD) {
        /* Return error */
        retval = -EPERM;
        goto leave;
    }

    /* Invalid priority parameter */
    if (param->sched_priority < 0 ||
        param->sched_priority > THREAD_PRIORITY_MAX) {
        /* Return error */
        retval = -EINVAL;
        goto leave;
    }

    /* Apply settings */
    if (thread->priority_inherited)
        thread->original_priority = param->sched_priority;
    else
        thread->priority = param->sched_priority;

    /* Return success */
    retval = 0;

leave:
    preempt_enable();
    return retval;
}

static int sys_pthread_getschedparam(pthread_t tid,
                                     int *policy,
                                     struct sched_param *param)
{
    preempt_disable();

    int retval;

    struct thread_info *thread = acquire_thread(tid);
    if (!thread) {
        /* Return error */
        retval = -ESRCH;
        goto leave;
    }

    /* kthread can only be set by the kernel */
    if (thread->privilege == KERNEL_THREAD) {
        /* Return error */
        retval = -EPERM;
        goto leave;
    }

    /* Return settings */
    if (thread->priority_inherited)
        param->sched_priority = thread->original_priority;
    else
        param->sched_priority = thread->priority;

    /* Return success */
    retval = 0;

leave:
    preempt_enable();
    return retval;
}

static int sys_pthread_yield(void)
{
    /* Yield the time quatum to other threads */
    prepare_to_wait(&sleep_list, running_thread, THREAD_WAIT);

    /* Return success */
    return 0;
}

static void handle_signal(struct thread_info *thread, int signum)
{
    bool stage_handler = false;

    /* Wake up the thread from the signal waiting list */
    if (thread->wait_for_signal && (sig2bit(signum) & thread->sig_wait_set)) {
        /* Disable signal waiting flag */
        thread->wait_for_signal = false;

        /* Wake up the thread and set the return values */
        finish_wait(thread);
        *thread->ret_sig = signum;
        SYSCALL_ARG(thread, int, 0) = 0;
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

static int sys_pthread_kill(pthread_t tid, int sig)
{
    preempt_disable();

    int retval;

    struct thread_info *thread = acquire_thread(tid);

    /* Failed to find the thread */
    if (!thread) {
        /* Return error */
        retval = -ESRCH;
        goto leave;
    }

    /* kthread does not receive signals */
    if (thread->privilege == KERNEL_THREAD) {
        /* Return error */
        retval = -EPERM;
        goto leave;
    }

    /* Check if the signal number is defined */
    if (!is_signal_defined(sig)) {
        /* Return error */
        retval = -EINVAL;
        goto leave;
    }

    handle_signal(thread, sig);

    /* Return success */
    retval = 0;

leave:
    preempt_enable();
    return retval;
}

static void sys_pthread_exit(void *retval)
{
    preempt_disable();

    running_thread->retval = retval;
    thread_join_handler();

    preempt_enable();
}

static int sys_pthread_mutex_unlock(pthread_mutex_t *_mutex)
{
    preempt_disable();

    struct mutex *mutex = (struct mutex *) _mutex;
    int retval = mutex_unlock(mutex);

    /* Handle priority inheritance */
    if (retval == 0 && mutex->protocol == PTHREAD_PRIO_INHERIT &&
        running_thread->priority_inherited) {
        /* Recover the original thread priority */
        running_thread->priority = running_thread->original_priority;
    }

    preempt_enable();

    return retval;
}

static void raise_thread_priority_of_mutex_owner(struct mutex *mutex)
{
    preempt_disable();

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

    preempt_enable();
}

static int sys_pthread_mutex_lock(pthread_mutex_t *_mutex)
{
    struct mutex *mutex = (struct mutex *) _mutex;

    int retval;
    while (1) {
        retval = __mutex_lock(mutex);

        if (retval == -ERESTARTSYS) {
            raise_thread_priority_of_mutex_owner(mutex);
        } else {
            break;
        }

        schedule();
    };

    if (mutex->protocol == PTHREAD_PRIO_INHERIT) {
        /* Preserve thread priority in case priority inheritance happens */
        running_thread->original_priority = running_thread->priority;
    }

    return retval;
}

static int sys_pthread_mutex_trylock(pthread_mutex_t *mutex)
{
    int retval = __mutex_lock((struct mutex *) mutex);
    return (retval == -ERESTARTSYS) ? -EBUSY : retval;
}

static int sys_pthread_cond_signal(pthread_cond_t *cond)
{
    /* Wake up a thread from the wait list */
    wake_up(&((struct cond *) cond)->task_wait_list);

    /* Return success */
    return 0;
}

static int sys_pthread_cond_broadcast(pthread_cond_t *cond)
{
    /* Wake up all threads from the wait list */
    wake_up_all(&((struct cond *) cond)->task_wait_list);

    /* Return success */
    return 0;
}

static int sys_pthread_cond_wait(pthread_cond_t *cond, pthread_mutex_t *mutex)
{
    preempt_disable();

    if (((struct mutex *) mutex)->owner) {
        /* Release the mutex */
        ((struct mutex *) mutex)->owner = NULL;

        /* Enqueue current thread into the read waiting list */
        prepare_to_wait(&((struct cond *) cond)->task_wait_list, running_thread,
                        THREAD_WAIT);
    }

    preempt_enable();

    /* Return success */
    return 0;
}

static int sys_pthread_once(pthread_once_t *_once_control,
                            void (*init_routine)(void))
{
    preempt_disable();

    struct thread_once *once_control = (struct thread_once *) _once_control;

    if (once_control->finished)
        goto leave;

    if (once_control->wait_list.next == NULL ||
        once_control->wait_list.prev == NULL) {
        /* The first time to execute pthread_once() */
        INIT_LIST_HEAD(&once_control->wait_list);
    } else {
        /* pthread_once() is already called */
        prepare_to_wait(&once_control->wait_list, running_thread, THREAD_WAIT);
        goto leave;
    }

    running_thread->once_control = once_control;
    stage_temporary_handler(running_thread, (uint32_t) init_routine,
                            (uint32_t) thread_once_return_handler, NULL);

leave:
    preempt_enable();
    return 0;
}

static int sys_sem_post(sem_t *sem)
{
    return up((struct semaphore *) sem);
}

static int sys_sem_trywait(sem_t *sem)
{
    return down_trylock((struct semaphore *) sem);
}

static int sys_sem_wait(sem_t *sem)
{
    return down((struct semaphore *) sem);
}

static int sys_sem_getvalue(sem_t *sem, int *sval)
{
    preempt_disable();
    *sval = ((struct semaphore *) sem)->count;
    preempt_enable();

    /* Return success */
    return 0;
}

static int sys_sigaction(int signum,
                         const struct sigaction *act,
                         struct sigaction *oldact)
{
    preempt_disable();

    int retval;

    /* Check if the signal number is defined */
    if (!is_signal_defined(signum)) {
        /* Return error */
        retval = -EINVAL;
        goto leave;
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
            retval = -ENOMEM;
            goto leave;
        }

        /* Register signal action on the table */
        running_thread->sig_table[sig_idx] = new_act;
        *new_act = *act;
    }

    /* Return success */
    retval = 0;

leave:
    preempt_enable();
    return retval;
}

static int sys_sigwait(const sigset_t *set, int *sig)
{
    preempt_disable();

    int retval;

    sigset_t invalid_mask =
        ~(sig2bit(SIGUSR1) | sig2bit(SIGUSR2) | sig2bit(SIGPOLL) |
          sig2bit(SIGSTOP) | sig2bit(SIGCONT) | sig2bit(SIGKILL));

    /* Reject waiting request of an undefined signal */
    if (*set & invalid_mask) {
        /* Return error */
        retval = -EINVAL;
        goto leave;
    }

    /* Record the signals to wait */
    running_thread->sig_wait_set = *set;
    running_thread->ret_sig = sig;
    running_thread->wait_for_signal = true;

    /* Enqueue the thread into the signal waiting list */
    prepare_to_wait(&suspend_list, running_thread, THREAD_WAIT);

    /* Return success */
    retval = 0;

leave:
    preempt_enable();
    return retval;
}

static int sys_kill(pid_t pid, int sig)
{
    preempt_disable();

    int retval;

    struct task_struct *task = acquire_task(pid);

    /* Failed to find the task */
    if (!task) {
        /* Return error */
        retval = -ESRCH;
        goto leave;
    }

    /* Check if the signal number is defined */
    if (!is_signal_defined(sig)) {
        /* Return error */
        retval = -EINVAL;
        goto leave;
    }

    struct list_head *curr, *next;
    list_for_each_safe (curr, next, &task->threads_list) {
        struct thread_info *thread =
            list_entry(curr, struct thread_info, task_list);
        handle_signal(thread, sig);
    }

    /* Return success */
    retval = 0;

leave:
    preempt_enable();
    return retval;
}

int sys_raise(int sig)
{
    preempt_disable();

    int retval;

    struct task_struct *task = current_task_info();

    /* Failed to find the task */
    if (!task) {
        /* Return error */
        retval = -ESRCH;
        goto leave;
    }

    /* Check if the signal number is defined */
    if (!is_signal_defined(sig)) {
        /* Return error */
        retval = -EINVAL;
        goto leave;
    }

    struct list_head *curr, *next;
    list_for_each_safe (curr, next, &task->threads_list) {
        struct thread_info *thread =
            list_entry(curr, struct thread_info, task_list);
        handle_signal(thread, sig);
    }

    /* Return success */
    retval = 0;

leave:
    preempt_enable();
    return retval;
}

static int sys_clock_gettime(clockid_t clockid, struct timespec *tp)
{
    preempt_disable();

    int retval;

    if (clockid != CLOCK_MONOTONIC) {
        /* Return error */
        retval = -EINVAL;
        goto leave;
    }

    get_sys_time(tp);

    /* Return success */
    retval = 0;

leave:
    preempt_enable();
    return retval;
}

static int sys_clock_settime(clockid_t clockid, const struct timespec *tp)
{
    preempt_disable();

    int retval;

    if (clockid != CLOCK_MONOTONIC) {
        /* Return error */
        retval = -EINVAL;
        goto leave;
    }

    set_sys_time(tp);

    /* Return success */
    retval = 0;

leave:
    preempt_enable();
    return retval;
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

static int sys_timer_create(clockid_t clockid,
                            struct sigevent *sevp,
                            timer_t *timerid)
{
    preempt_disable();

    int retval;

    /* Unsupported clock source */
    if (clockid != CLOCK_MONOTONIC) {
        /* Return error */
        retval = -EINVAL;
        goto leave;
    }

    if (sevp->sigev_notify != SIGEV_NONE &&
        sevp->sigev_notify != SIGEV_SIGNAL) {
        /* Return error */
        retval = -EINVAL;
        goto leave;
    }

    /* Allocate memory for the new timer */
    struct timer *new_tm = kmalloc(sizeof(struct timer));

    /* Failed to allocate memory */
    if (new_tm == NULL) {
        /* Return error */
        retval = -ENOMEM;
        goto leave;
    }

    /* Record timer settings */
    new_tm->id = running_thread->timer_cnt;
    new_tm->sev = *sevp;
    new_tm->thread = running_thread;

    /* Initialize thread timer list */
    if (running_thread->timer_cnt == 0)
        INIT_LIST_HEAD(&running_thread->timers_list);

    /* Link the new timer to the list */
    list_add(&new_tm->g_list, &timers_list);
    list_add(&new_tm->list, &running_thread->timers_list);

    /* Return timer ID */
    *timerid = running_thread->timer_cnt;

    /* Increase timer count */
    running_thread->timer_cnt++;

    /* Return success */
    retval = 0;

leave:
    preempt_enable();
    return retval;
}

static int sys_timer_delete(timer_t timerid)
{
    preempt_disable();

    int retval;

    /* Aquire the timer with given ID */
    struct timer *timer = acquire_timer(timerid);

    /* Failed to acquire the timer */
    if (timer == NULL) {
        /* Invalid timer ID, return error */
        retval = -EINVAL;
        goto leave;
    }

    /* Remove the timer from the lists and free the memory */
    list_del(&timer->g_list);
    list_del(&timer->list);
    kfree(timer);

    /* Return success */
    retval = 0;

leave:
    preempt_enable();
    return retval;
}

static int sys_timer_settime(timer_t timerid,
                             int flags,
                             const struct itimerspec *new_value,
                             struct itimerspec *old_value)
{
    preempt_disable();

    int retval;

    /* Bad arguments */
    if (new_value->it_value.tv_sec < 0 || new_value->it_value.tv_nsec < 0 ||
        new_value->it_value.tv_nsec > 999999999) {
        /* Return error */
        retval = -EINVAL;
        goto leave;
    }

    /* The thread has no timer */
    if (running_thread->timer_cnt == 0) {
        /* Return error */
        retval = -EINVAL;
        goto leave;
    }

    /* Acquire the timer with given ID */
    struct timer *timer = acquire_timer(timerid);

    /* Failed to acquire the timer */
    if (timer == NULL) {
        /* Return error */
        retval = -EINVAL;
        goto leave;
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

    /* Return success */
    retval = 0;

leave:
    preempt_enable();
    return retval;
}

static int sys_timer_gettime(timer_t timerid, struct itimerspec *curr_value)
{
    preempt_disable();

    int retval;

    /* Acquire the timer with given ID */
    struct timer *timer = acquire_timer(timerid);

    /* Failed to acquire the timer */
    if (timer == NULL) {
        /* invalid timer ID, return error */
        retval = -EINVAL;
        goto leave;
    }

    *curr_value = timer->ret_time;

    /* Return success */
    retval = 0;

leave:
    preempt_enable();
    return retval;
}

static void *sys_malloc(size_t size)
{
    preempt_disable();

    void *ptr;

    /* Return NULL if the size is zero */
    if (size == 0) {
        ptr = NULL;
        goto leave;
    }

    ptr = __malloc(size);

leave:
    preempt_enable();
    return ptr;
}

static void sys_free(void *ptr)
{
    preempt_disable();

    /* Free the memory */
    __free(ptr);

    preempt_enable();
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
            finish_wait(thread);
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

static inline void set_need_resched(void)
{
    need_resched_flag = true;
}

static inline void reset_need_resched(void)
{
    need_resched_flag = false;
}

static inline bool need_resched(void)
{
    return need_resched_flag;
}

static void syscall_return_event_handler(void)
{
    running_thread->stack_top =
        (unsigned long *) running_thread->syscall_stack_top;
    running_thread->privilege =
        running_thread->kernel_thread ? KERNEL_THREAD : USER_THREAD;
    running_thread->syscall_mode = false;

    set_need_resched();
}

static inline void signal_cleanup_event_handler(void)
{
    running_thread->stack_top =
        (unsigned long *) running_thread->stack_top_preserved;
    running_thread->stack_top_preserved = (unsigned long) NULL;

    set_need_resched();
}

static void thread_return_event_handler(void)
{
    struct task_struct *task = current_task_info();
    if (running_thread == task->main_thread) {
        /* Returned from the main thread, thus the whole task should
         * be terminated */
        int status = SYSCALL_ARG(running_thread, int, 0);
        sys_exit(status);
    } else {
        running_thread->retval = SYSCALL_ARG(running_thread, void *, 0);
        thread_join_handler();
    }

    set_need_resched();
}

static void pthread_once_event_handler(void)
{
    /* Restore the stack */
    running_thread->stack_top =
        (unsigned long *) running_thread->stack_top_preserved;
    running_thread->stack_top_preserved = (unsigned long) NULL;

    /* Wake up all waiting threads and mark once variable as complete */
    struct thread_once *once_control = running_thread->once_control;
    once_control->finished = true;
    wake_up_all(&once_control->wait_list);

    set_need_resched();
}

static void setup_syscall(struct thread_info *thread,
                          uint32_t func,
                          uint32_t return_handler,
                          uint32_t args[4])
{
    thread->syscall_stack_top = thread->stack_top;
    __stack_init((uint32_t **) &thread->stack_top, func, return_handler, args);
}

/* Syscall table */
static struct syscall_info syscall_table[] = {SYSCALL_TABLE_INIT};

static void syscall_handler(void)
{
    unsigned long syscall_num = get_syscall_num(running_thread->stack_top);

    /* Match request with system event table */
    switch (syscall_num) {
    case 0:
        return;
    case SYSCALL_RETURN_EVENT:
        syscall_return_event_handler();
        return;
    case SIGNAL_CLEANUP_EVENT:
        signal_cleanup_event_handler();
        return;
    case THREAD_RETURN_EVENT:
        thread_return_event_handler();
        return;
    case THREAD_ONCE_EVENT:
        pthread_once_event_handler();
        return;
    }

    /* Match request with system call table */
    for (int i = 0; i < SYSCALL_CNT; i++) {
        if (syscall_num == syscall_table[i].num) {
            if (running_thread->syscall_mode)
                return;

            get_syscall_args(running_thread->stack_top,
                             running_thread->syscall_args);

            setup_syscall(running_thread, syscall_table[i].handler_func,
                          (uint32_t) syscall_return_handler,
                          *running_thread->syscall_args);

            running_thread->privilege = KERNEL_THREAD;
            running_thread->syscall_mode = true;

            return;
        }
    }

    /* Unknown request */
    panic(
        "\r=============== SYSCALL FAULT ================\n\r"
        "Current thread: %p (%s)\n\r"
        "Faulting syscall number = %d\n\r"
        "Halting system\n\r"
        "==============================================",
        running_thread, running_thread->name, syscall_num);
}

static void __schedule(void)
{
    /* Stop current thread */
    if (running_thread->status == THREAD_RUNNING)
        prepare_to_wait(&sleep_list, running_thread, THREAD_WAIT);

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

    /* Check if the thread has pending signals */
    if (!running_thread->syscall_mode)
        check_pending_signals();
}

void schedule(void)
{
    __preempt_disable();

    /* Record nesting level of the critical section */
    int nesting = preempt_count();

    /* Jump back to the kernel loop and reschedule */
    set_need_resched();
    jump_to_kernel();

    __preempt_enable();

    /* Turn of the interrupt again if necessary */
    if (nesting)
        __preempt_disable();
}

static void print_platform_info(void)
{
#ifndef BUILD_QEMU
    /* Clear screen */
    char *cls_str = "\x1b[H\x1b[2J";
    console_write("\x1b[H\x1b[2J", strlen(cls_str));
#endif
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
        panic(
            "\r=============== STACK OVERFLOW ===============\n\r"
            "Current thread: %p (%s)\n\r"
            "Stack range: [0x%08x-0x%08x]\n\r"
            "Stack size: %d\n\r"
            "Faulting stack pointer = %p\n\r"
            "Halting system\n\r"
            "==============================================",
            running_thread, running_thread->name, lower_bound, upper_bound,
            running_thread->stack_size, running_thread->stack_top);
    }
}

static void *init(void *arg)
{
    /* Bring up drivers */
    preempt_disable();
    {
        print_platform_info();
        __board_init();
        rom_dev_init();
        null_dev_init();
        link_stdin_dev(STDIN_PATH);
        link_stdout_dev(STDOUT_PATH);
        link_stderr_dev(STDERR_PATH);
        printkd_start();
    }
    preempt_enable();

    /* Mount rom file system */
    mount("/dev/rom", "/");

    /* Acquire list of all hooked user tasks */
    extern char _tasks_start, _tasks_end;
    struct task_hook *tasks = (struct task_hook *) &_tasks_start;
    size_t task_cnt = ((uintptr_t) &_tasks_end - (uintptr_t) &_tasks_start) /
                      sizeof(struct task_hook);

    /* Launched all hooked user tasks */
    for (size_t i = 0; i < task_cnt; i++)
        task_create(tasks[i].task_func, tasks[i].priority, tasks[i].stacksize);

    return 0;
}

static void start_init_thread(void)
{
    /* Launch system initialization thread */
    pthread_attr_t attr;
    struct sched_param param;
    param.sched_priority = KTHREAD_PRI_MAX;
    pthread_attr_init(&attr);
    pthread_attr_setschedparam(&attr, &param);
    pthread_attr_setschedpolicy(&attr, SCHED_RR);
    pthread_attr_setstacksize(&attr, INIT_STACK_SIZE);

    pthread_t tid;
    if (pthread_create(&tid, &attr, init, NULL) < 0) {
        halt(); /* Unexpected error */
    }

    /* Wait until initialization finished */
    pthread_join(tid, NULL);
}

static void idle(void)
{
    setprogname("idle");

    /* Perform system initialization */
    start_init_thread();

    /* Run idle loop when nothing to do */
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
    printkd_init();
    rootfs_init();

    /* Initialize ready lists */
    for (int i = 0; i <= KTHREAD_PRI_MAX; i++) {
        INIT_LIST_HEAD(&ready_list[i]);
    }

    /* Create kernel threads for basic services */
    kthread_create(idle, 0, IDLE_STACK_SIZE);
    kthread_create(softirqd, KTHREAD_PRI_MAX, SOFTIRQD_STACK_SIZE);
    kthread_create(filesysd, KTHREAD_PRI_MAX - 1, FILESYSD_STACK_SIZE);
    kthread_create(printkd, KTHREAD_PRI_MAX - 1, PRINTKD_STACK_SIZE);

    /* Dequeue and execute the init thread */
    running_thread = &threads[0];
    threads[0].status = THREAD_RUNNING;
    list_del(&threads[0].list);

    while (1) {
        /* Capture SysTick and Supervisor Call events */
        if (check_systick_event(running_thread->stack_top)) {
            system_ticks_update();
            set_need_resched();
        } else {
            syscall_handler();
        }

        /* Rescheduling */
        if (need_resched()) {
            reset_need_resched();
            __schedule();
        }

        /* Check thread stack pointer to detect stack overflow */
        check_thread_stack();

        /* Jump to the selected thread */
        running_thread->stack_top = jump_to_thread(running_thread->stack_top,
                                                   running_thread->privilege);
    }
}
