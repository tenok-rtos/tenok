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
#include <fs/reg_file.h>
#include <fs/rom_dev.h>
#include <fs/vfs.h>
#include <kernel/daemon.h>
#include <kernel/errno.h>
#include <kernel/kernel.h>
#include <kernel/kfifo.h>
#include <kernel/mqueue.h>
#include <kernel/mutex.h>
#include <kernel/pipe.h>
#include <kernel/preempt.h>
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

/* System call */
static bool syscall_flag;

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
struct file *files[STD_STREAM_CNT + FILE_MAX];
struct file *thread_pipe[THREAD_MAX];
int file_cnt;

/* File descriptor table */
/* A descriptor is its own index into this table, the standard streams among
 * them, so there is nothing to add or subtract anywhere
 */
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

void preempt_count_inc(void)
{
    preempt_cnt++;
}

void preempt_count_dec(void)
{
    if (preempt_cnt > 0)
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

int preempt_count(void)
{
    return preempt_cnt;
}

void preempt_count_set(uint32_t count)
{
    preempt_cnt = count;
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
        int page_order = size_to_page_order(alloc_size);
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
        ((struct kmalloc_header *) ptr)->alloc_size = alloc_size;
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
    size_t alloc_size = addr->alloc_size;

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

/* The file a descriptor of the running task refers to, and a null pointer if
 * it refers to nothing
 */
static struct file *fd_file(struct task_struct *task, int fd)
{
    if (fd < 0 || fd >= OPEN_MAX)
        return NULL;

    if (!bitmap_get_bit(bitmap_fds, fd) ||
        !bitmap_get_bit(task->bitmap_fds, fd))
        return NULL;

    struct file *filp = fdtable[fd].file;

    filp->f_flags = fdtable[fd].flags;

    return filp;
}

/* A pipe belongs to nothing but the descriptors that name it, so every one
 * that is handed out or given up is counted. Every other file is owned by the
 * file system and outlives the descriptors that name it.
 */
static void fd_pipe_take(int fd)
{
    if (file_is_pipe(fdtable[fd].file))
        pipe_take(fdtable[fd].file,
                  (fdtable[fd].flags & O_ACCMODE) == O_WRONLY);
}

static void fd_pipe_give_up(int fd)
{
    if (file_is_pipe(fdtable[fd].file))
        pipe_give_up(fdtable[fd].file,
                     (fdtable[fd].flags & O_ACCMODE) == O_WRONLY);
}

/* Give the task the lowest descriptor it does not already hold, which is what
 * POSIX promises a caller that closes one and opens another
 */
static int fd_take(struct task_struct *task, struct file *filp, int flags)
{
    int fd = find_first_zero_bit(bitmap_fds, OPEN_MAX);

    if (fd >= OPEN_MAX)
        return -ENFILE;

    bitmap_set_bit(bitmap_fds, fd);
    bitmap_set_bit(task->bitmap_fds, fd);

    fdtable[fd].file = filp;
    fdtable[fd].flags = flags;
    fdtable[fd].fd_flags = (flags & O_CLOEXEC) ? FD_CLOEXEC : 0;
    fd_pipe_take(fd);

    return fd;
}

static void fd_give_up(struct task_struct *task, int fd)
{
    /* A descriptor that is not held names nothing to give up */
    if (bitmap_get_bit(bitmap_fds, fd) && bitmap_get_bit(task->bitmap_fds, fd))
        fd_pipe_give_up(fd);

    bitmap_clear_bit(bitmap_fds, fd);
    bitmap_clear_bit(task->bitmap_fds, fd);
}

/* A task starts with the three streams a program expects to find open */
static void fd_open_std_streams(struct task_struct *task)
{
    for (int fd = 0; fd < STD_STREAM_CNT; fd++) {
        bitmap_set_bit(bitmap_fds, fd);
        bitmap_set_bit(task->bitmap_fds, fd);

        fdtable[fd].file = files[fd];
        fdtable[fd].flags = 0;
        fdtable[fd].fd_flags = 0;
    }
}

static inline struct task_struct *current_task_info(void)
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
        if (thread->tid == tid && thread->status != THREAD_TERMINATED)
            return thread;
    }

    return NULL;
}

/* A thread that has ended but still holds its number, waiting for someone to
 * ask it for its return value
 */
static struct thread_info *acquire_ended_thread(int tid)
{
    struct thread_info *thread;
    list_for_each_entry (thread, &threads_list, thread_list) {
        if (thread->tid == tid && thread->status == THREAD_TERMINATED)
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
    size_t buf_size = ALIGN(sizeof(char) * THREAD_PIPE_BUF, sizeof(long));

    struct pipe *pipe = (struct pipe *) ((uintptr_t) stack_top - pipe_size);
    struct kfifo *pipe_fifo = (struct kfifo *) ((uintptr_t) pipe - kfifo_size);
    char *buf = (char *) ((uintptr_t) pipe_fifo - buf_size);

    kfifo_init(pipe_fifo, buf, sizeof(char), THREAD_PIPE_BUF);
    pipe->fifo = pipe_fifo;
    thread_pipe[tid] = fifo_init(NULL, pipe);

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

    /* The procedure call standard of ARM requires the stack pointer to be
     * eight byte aligned at every public interface. The regions carved out
     * above are only aligned to a word, and a stack that is off by four
     * breaks the alignment va_arg() applies before it reads a 64 bit
     * argument: every "%lld" would then read one word too early.
     */
    thread->stack_top =
        (unsigned long *) ((uintptr_t) thread->stack_top & ~(uintptr_t) 7);

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
    list_add_tail(&thread->thread_list, &threads_list);

    /* Enqueue the thread into the sleep list */
    list_add_tail(&thread->list, &sleep_list);

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
    task->umask = FS_DEFAULT_UMASK;
    fd_open_std_streams(task);
    INIT_LIST_HEAD(&task->threads_list);
    list_add_tail(&thread->task_list, &task->threads_list);
    list_add_tail(&task->list, &tasks_list);

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

static void enqueue_pending_signal(struct thread_info *thread,
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
    list_move_tail(&thread->list, &ready_list[thread->priority]);
}

/* Give back everything a thread was still holding after it ended: its number,
 * its place in the lists, and the memory its stack was in
 */
static void thread_reap(struct thread_info *thread)
{
    list_del(&thread->thread_list);
    list_del(&thread->task_list);
    bitmap_clear_bit(bitmap_threads, thread->tid);

    free_pages((uint32_t) thread->stack,
               size_to_page_order(thread->stack_size));
}

static void thread_delete(struct thread_info *thread)
{
    /* Remove the thread from the system */
    if (thread != running_thread)
        list_del(&thread->list);
    thread->status = THREAD_TERMINATED;
    thread_reap(thread);

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

    list_add_tail(&thread->list, wait_list);
    thread->status = state;

    preempt_enable();
}

void finish_wait(struct thread_info *thread)
{
    preempt_disable();

    if (thread != running_thread) {
        thread->status = THREAD_READY;
        list_move_tail(&thread->list, &ready_list[thread->priority]);
    }

    preempt_enable();
}

void wake_up(struct list_head *wait_list)
{
    preempt_disable();

    if (list_empty(wait_list))
        goto leave;

    struct thread_info *highest_pri_thread =
        list_first_entry(wait_list, struct thread_info, list);

    /* Find the first highest-priority thread in the waiting list */
    struct thread_info *thread;
    list_for_each_entry (thread, wait_list, list) {
        if (thread->priority > highest_pri_thread->priority)
            highest_pri_thread = thread;
    }

    /* Wake up the first highest-priority thread in the waiting list */
    list_move_tail(&highest_pri_thread->list,
                   &ready_list[highest_pri_thread->priority]);
    highest_pri_thread->status = THREAD_READY;

leave:
    preempt_enable();
}

void wake_up_all(struct list_head *wait_list)
{
    preempt_disable();

    /* Wake up all threads from the waiting list */
    struct list_head *curr, *next;
    list_for_each_safe (curr, next, wait_list) {
        struct thread_info *thread = list_entry(curr, struct thread_info, list);

        list_move_tail(&thread->list, &ready_list[thread->priority]);
        thread->status = THREAD_READY;
    }

    preempt_enable();
}

static inline void thread_join_handler(void)
{
    bool asked_for = false;

    /* Wake up the threads that waiting to join */
    struct list_head *curr, *next;
    list_for_each_safe (curr, next, &running_thread->join_list) {
        struct thread_info *thread = list_entry(curr, struct thread_info, list);

        /* Pass the return value back to the joining thread */
        if (thread->retval_join) {
            *thread->retval_join = running_thread->retval;
        }

        finish_wait(thread);
        asked_for = true;
    }

    if (running_thread->joinable && !running_thread->detached && !asked_for) {
        /* Nobody has asked for the return value yet, and a joinable thread has
         * to have one to give when somebody does. So the thread stays where it
         * is, holding its number and its answer, until pthread_join() or
         * pthread_detach() says it is no longer needed
         */
        running_thread->status = THREAD_TERMINATED;
        return;
    }

    thread_delete(running_thread);
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
    strncpy(running_thread->name, name, THREAD_NAME_MAX - 1);
    running_thread->name[THREAD_NAME_MAX - 1] = '\0';
}

static int sys_delay_ticks(uint32_t ticks)
{
    preempt_disable();

    /* Reconfigure the tick to sleep */
    running_thread->sleep_ticks = ticks;

    /* Enqueue the thread into the sleep list */
    running_thread->status = THREAD_WAIT;
    list_add_tail(&(running_thread->list), &sleep_list);

    preempt_enable();

    /* Return success */
    return 0;
}

static bool timespec_valid(const struct timespec *ts)
{
    return ts && ts->tv_sec >= 0 && ts->tv_nsec >= 0 &&
           ts->tv_nsec < 1000000000L;
}

static int timespec_cmp(const struct timespec *a, const struct timespec *b)
{
    if (a->tv_sec == b->tv_sec) {
        if (a->tv_nsec == b->tv_nsec)
            return 0;
        return (a->tv_nsec > b->tv_nsec) ? 1 : -1;
    }
    return (a->tv_sec > b->tv_sec) ? 1 : -1;
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

/* Check if a file is still referenced by an open file descriptor. The file
 * system uses it to refuse removing a file that is currently open.
 */
bool file_is_opened(struct file *filp)
{
    for (int i = 0; i < OPEN_MAX; i++) {
        if (bitmap_get_bit(bitmap_fds, i) && (fdtable[i].file == filp))
            return true;
    }

    return false;
}

static int sys_mount(const char *source, const char *target)
{
    /* Check the length of the pathname */
    if (strlen(source) >= PATH_MAX || strlen(target) >= PATH_MAX)
        return -ENAMETOOLONG;

    int tid = running_thread->tid;
    return vfs_mount(tid, source, target);
}

/* The permission bits a task is allowed to give a file it creates */
static mode_t apply_umask(mode_t mode)
{
    return mode & ~current_task_info()->umask & 07777;
}

static int sys_open(const char *pathname, int flags, mode_t mode)
{
    preempt_disable();

    int retval;

    if (!pathname) {
        retval = -EFAULT;
        goto err;
    }

    /* Check the length of the pathname */
    if (strlen(pathname) >= PATH_MAX) {
        retval = -ENAMETOOLONG;
        goto err;
    }

    /* Acquire the running task */
    struct task_struct *task = current_task_info();

    /* Acquire the thread ID */
    int tid = running_thread->tid;

    int file_idx = vfs_open_file(tid, pathname);

    if ((file_idx == -ENOENT) && (flags & O_CREAT)) {
        /* Create the file as it does not exist yet */
        file_idx = vfs_create_file(tid, pathname, S_IFREG | apply_umask(mode));
    } else if ((file_idx >= 0) && (flags & O_CREAT) && (flags & O_EXCL)) {
        /* O_EXCL requires the file to be created by this very call */
        retval = -EEXIST;
        goto err;
    }

    /* File not found */
    if (file_idx < 0) {
        /* Return error */
        retval = file_idx;
        goto err;
    }

    struct file *filp = files[file_idx];

    /* A caller that asks for a directory is told when it did not get one */
    if ((flags & O_DIRECTORY) && filp->f_inode &&
        !S_ISDIR(filp->f_inode->i_mode)) {
        retval = -ENOTDIR;
        goto err;
    }

    /* Take a descriptor for the file */
    int fd = fd_take(task, filp, flags);
    if (fd < 0) {
        retval = fd;
        goto err;
    }

    /* Check if the file operation is undefined */
    if (!filp->f_op->open) {
        fd_give_up(task, fd);

        /* Return error */
        retval = -ENXIO;
        goto err;
    }

    /* Apply the open flags on a regular file. Note that the file position
     * is owned by the file rather than by the file descriptor, so opening a
     * file has to reset it.
     */
    if (filp->f_inode && S_ISREG(filp->f_inode->i_mode)) {
        if (flags & O_TRUNC) {
            if (filp->f_inode->i_rdev != RDEV_ROOTFS) {
                fd_give_up(task, fd);

                /* Return error */
                retval = -EROFS;
                goto err;
            }

            reg_file_truncate(filp);
        } else if (flags & O_APPEND) {
            reg_file_seek_end(filp);
        } else {
            reg_file_rewind(filp);
        }
    }

    preempt_enable();

    /* Call open operation  */
    filp->f_op->open(filp->f_inode, filp);

    /* Return the file descriptor number */
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

    /* Check if the file descriptor belongs to current task */
    if (!fd_file(task, fd)) {
        retval = -EBADF;
        goto leave;
    }

    /* Free the file descriptor */
    fd_give_up(task, fd);

    /* Return success */
    retval = 0;

leave:
    preempt_enable();
    return retval;
}

/* A program of Tenok reaches memory where it already is: there is no address
 * space to put a second name for it in. So the call asks the file whether it
 * has somewhere to be reached, and hands back that, or fails
 */
static void *sys_mmap(int fd, size_t length, off_t offset)
{
    preempt_disable();

    void *retval;
    struct task_struct *task = current_task_info();

    struct file *filp = fd_file(task, fd);
    if (!filp) {
        retval = (void *) -EBADF;
        goto leave;
    }

    if (!filp->f_op->mmap) {
        retval = (void *) -ENODEV;
        goto leave;
    }

    preempt_enable();

    return filp->f_op->mmap(filp, length, offset);

leave:
    preempt_enable();
    return retval;
}

static int sys_pipe(int pipefd[2])
{
    preempt_disable();

    int retval;

    if (!pipefd) {
        retval = -EFAULT;
        goto err;
    }

    /* Acquire the running task */
    struct task_struct *task = current_task_info();

    struct file *filp = pipe_alloc();
    if (!filp) {
        retval = -ENFILE;
        goto err;
    }

    /* One end is read from and the other is written to, and both name the
     * same pipe
     */
    int read_fd = fd_take(task, filp, O_RDONLY);
    if (read_fd < 0) {
        retval = read_fd;
        goto undo_pipe;
    }

    int write_fd = fd_take(task, filp, O_WRONLY);
    if (write_fd < 0) {
        retval = write_fd;
        goto undo_read_fd;
    }

    pipefd[0] = read_fd;
    pipefd[1] = write_fd;

    preempt_enable();
    return 0;

undo_read_fd:
    fd_give_up(task, read_fd);
    goto err;
undo_pipe:
    pipe_release(filp);
err:
    preempt_enable();
    return retval;
}

static int sys_dup(int oldfd)
{
    preempt_disable();

    int retval;

    /* Acquire the running task */
    struct task_struct *task = current_task_info();

    /* Check if the file descriptor is invalid */
    if (!fd_file(task, oldfd)) {
        retval = -EBADF;
        goto leave;
    }

    /* The copy refers to the same file and carries the same flags */
    retval = fd_take(task, fdtable[oldfd].file, fdtable[oldfd].flags);

leave:
    preempt_enable();
    return retval;
}

/* Take the lowest descriptor at or above the one asked for, which is what
 * F_DUPFD promises and what a shell saves a descriptor with
 */
static int fd_take_above(struct task_struct *task,
                         struct file *filp,
                         int flags,
                         int lowest)
{
    if (lowest < 0 || lowest >= OPEN_MAX)
        return -EINVAL;

    for (int fd = lowest; fd < OPEN_MAX; fd++) {
        if (bitmap_get_bit(bitmap_fds, fd))
            continue;

        bitmap_set_bit(bitmap_fds, fd);
        bitmap_set_bit(task->bitmap_fds, fd);

        fdtable[fd].file = filp;
        fdtable[fd].flags = flags;
        fdtable[fd].fd_flags = (flags & O_CLOEXEC) ? FD_CLOEXEC : 0;
        fd_pipe_take(fd);

        return fd;
    }

    return -EMFILE;
}

static int sys_fcntl(int fd, int cmd, unsigned long arg)
{
    preempt_disable();

    int retval;
    struct task_struct *task = current_task_info();

    if (!fd_file(task, fd)) {
        retval = -EBADF;
        goto leave;
    }

    switch (cmd) {
    case F_DUPFD:
    case F_DUPFD_CLOEXEC:
        retval =
            fd_take_above(task, fdtable[fd].file, fdtable[fd].flags, (int) arg);

        /* The copy is closed on exec only when it was asked to be, whatever
         * the descriptor it came from does
         */
        if (retval >= 0)
            fdtable[retval].fd_flags =
                (cmd == F_DUPFD_CLOEXEC) ? FD_CLOEXEC : 0;
        break;
    case F_GETFD:
        /* Tenok has no exec() for the close on exec bit to act on, it is
         * stored so that what a program sets it can read back
         */
        retval = fdtable[fd].fd_flags;
        break;
    case F_SETFD:
        fdtable[fd].fd_flags = (int) arg & FD_CLOEXEC;
        retval = 0;
        break;
    case F_GETFL:
        retval = fdtable[fd].flags;
        break;
    case F_SETFL:
        /* The access mode of an open file cannot be changed */
        fdtable[fd].flags &= (O_ACCMODE | FD_CLOEXEC);
        fdtable[fd].flags |= (int) arg & ~(O_ACCMODE | FD_CLOEXEC);
        retval = 0;
        break;
    default:
        retval = -EINVAL;
        break;
    }

leave:
    preempt_enable();
    return retval;
}

static int sys_dup2(int oldfd, int newfd)
{
    preempt_disable();

    int retval;

    /* Acquire the running task */
    struct task_struct *task = current_task_info();

    /* Check if the file descriptor to copy is invalid */
    if (!fd_file(task, oldfd)) {
        retval = -EBADF;
        goto leave;
    }

    /* Copying a descriptor onto itself changes nothing */
    if (oldfd == newfd) {
        retval = newfd;
        goto leave;
    }

    if (newfd < 0 || newfd >= OPEN_MAX) {
        retval = -EBADF;
        goto leave;
    }

    /* The one being replaced is closed, the way POSIX asks */
    fd_give_up(task, newfd);

    bitmap_set_bit(bitmap_fds, newfd);
    bitmap_set_bit(task->bitmap_fds, newfd);

    /* Copy the old file descriptor content to the new one. The copy is not
     * closed on exec even when the one it came from is, which is what POSIX
     * asks of dup2()
     */
    fdtable[newfd] = fdtable[oldfd];
    fdtable[newfd].fd_flags = 0;
    fd_pipe_take(newfd);

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

    if (!buf && count > 0) {
        retval = -EFAULT;
        goto err;
    }

    /* Acquire the running task */
    struct task_struct *task = current_task_info();

    /* Get the file to read */
    struct file *filp = fd_file(task, fd);
    if (!filp) {
        retval = -EBADF;
        goto err;
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
        retval = filp->f_op->read(filp, buf, count, filp->f_pos);

        if (retval != -ERESTARTSYS)
            break;

        schedule();
    }

    if (retval > 0)
        filp->f_pos += retval;

    return retval;

err:
    preempt_enable();
    return retval;
}

static ssize_t sys_write(int fd, const void *buf, size_t count)
{
    ssize_t retval;

    preempt_disable();

    if (!buf && count > 0) {
        retval = -EFAULT;
        goto err;
    }

    /* Acquire the running task */
    struct task_struct *task = current_task_info();

    /* Get the file pointer */
    struct file *filp = fd_file(task, fd);
    if (!filp) {
        retval = -EBADF;
        goto err;
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
        retval = filp->f_op->write(filp, buf, count, filp->f_pos);

        if (retval != -ERESTARTSYS)
            break;

        schedule();
    };

    if (retval > 0)
        filp->f_pos += retval;

    return retval;

err:
    preempt_enable();
    return retval;
}

static int sys_ioctl(int fd, unsigned long request, unsigned long arg)
{
    int retval;

    preempt_disable();

    /* Acquire the running task */
    struct task_struct *task = current_task_info();

    /* Get the file pointer */
    struct file *filp = fd_file(task, fd);
    if (!filp) {
        retval = -EBADF;
        goto err;
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
    struct file *filp = fd_file(task, fd);
    if (!filp) {
        retval = -EBADF;
        goto err;
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
    preempt_disable();

    int retval;

    if (!statbuf) {
        retval = -EFAULT;
        goto leave;
    }

    /* Acquire the running task */
    struct task_struct *task = current_task_info();

    /* Check if the file descriptor is invalid */
    struct file *filp = fd_file(task, fd);
    if (!filp) {
        retval = -EBADF;
        goto leave;
    }

    /* Get file inode */
    struct inode *inode = filp->f_inode;

    /* Check if the inode exists */
    if (inode != NULL) { /* XXX */
        fs_fill_stat(statbuf, inode);
    }

    /* Return success */
    retval = 0;

leave:
    preempt_enable();
    return retval;
}

static int sys_opendir(const char *pathname, DIR *dirp /* FIXME */)
{
    if (!pathname || !dirp)
        return -EFAULT;

    /* Check the length of the pathname */
    if (strlen(pathname) >= PATH_MAX)
        return -ENAMETOOLONG;

    int tid = running_thread->tid;

    return vfs_open_dir(tid, pathname, dirp);
}

static int sys_readdir(DIR *dirp, struct dirent *dirent)
{
    if (!dirp || !dirent)
        return -EFAULT;

    preempt_disable();
    int retval = vfs_readdir(dirp, dirent);
    preempt_enable();

    return retval;
}

/* Reading a directory walks a list of the entries it holds, and starting over
 * is putting the walk back at the front of that list
 */
static int sys_rewinddir(DIR *dirp)
{
    if (!dirp)
        return -EFAULT;

    if (!dirp->inode_dir)
        return -EBADF;

    dirp->dentry_list = dirp->inode_dir->i_dentry.next;

    return 0;
}

static char *sys_getcwd(char *buf, size_t size)
{
    int tid = running_thread->tid;
    return vfs_getcwd(tid, buf, size);
}

static int sys_chdir(const char *path)
{
    int tid = running_thread->tid;
    return vfs_chdir(tid, path);
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

    int file_idx =
        vfs_create_file(tid, pathname, (mode & S_IFMT) | apply_umask(mode));

    if (file_idx == -1) {
        /* Failed to create file */
        return -1; /* TODO: Specify the failed reason */
    } else {
        /* Return success */
        return 0;
    }
}

static int sys_mkdir(const char *pathname, mode_t mode)
{
    if (!pathname)
        return -EFAULT;

    /* Check the length of the pathname */
    if (strlen(pathname) >= PATH_MAX)
        return -ENAMETOOLONG;

    return vfs_mkdir(running_thread->tid, pathname, apply_umask(mode));
}

static int sys_rename(const char *oldpath, const char *newpath)
{
    if (!oldpath || !newpath)
        return -EFAULT;

    /* Check the length of the pathnames */
    if (strlen(oldpath) >= PATH_MAX || strlen(newpath) >= PATH_MAX)
        return -ENAMETOOLONG;

    return vfs_rename(running_thread->tid, oldpath, newpath);
}

static int sys_stat(const char *pathname, struct stat *statbuf)
{
    if (!pathname || !statbuf)
        return -EFAULT;

    /* Check the length of the pathname */
    if (strlen(pathname) >= PATH_MAX)
        return -ENAMETOOLONG;

    return vfs_stat(running_thread->tid, pathname, statbuf);
}

static mode_t sys_umask(mode_t mask)
{
    struct task_struct *task = current_task_info();
    mode_t previous = task->umask;

    task->umask = mask & 07777;

    return previous;
}

static int sys_utime(const char *pathname, uint32_t mtime)
{
    if (!pathname)
        return -EFAULT;

    /* Check the length of the pathname */
    if (strlen(pathname) >= PATH_MAX)
        return -ENAMETOOLONG;

    /* The caller asks for this moment by naming no other */
    if (mtime == UTIME_TO_NOW)
        mtime = fs_now();

    return vfs_utime(running_thread->tid, pathname, mtime);
}

static int sys_chmod(const char *pathname, mode_t mode)
{
    if (!pathname)
        return -EFAULT;

    /* Check the length of the pathname */
    if (strlen(pathname) >= PATH_MAX)
        return -ENAMETOOLONG;

    return vfs_chmod(running_thread->tid, pathname, mode);
}

static int sys_rmdir(const char *pathname)
{
    if (!pathname)
        return -EFAULT;

    /* Check the length of the pathname */
    if (strlen(pathname) >= PATH_MAX)
        return -ENAMETOOLONG;

    return vfs_remove(running_thread->tid, pathname, true);
}

static int sys_unlink(const char *pathname)
{
    if (!pathname)
        return -EFAULT;

    /* Check the length of the pathname */
    if (strlen(pathname) >= PATH_MAX)
        return -ENAMETOOLONG;

    return vfs_remove(running_thread->tid, pathname, false);
}

static int sys_mkfifo(const char *pathname, mode_t mode)
{
    /* Check the length of the pathname */
    if (strlen(pathname) >= PATH_MAX)
        return -ENAMETOOLONG;

    int tid = running_thread->tid;

    int file_idx = vfs_create_file(tid, pathname, S_IFIFO | apply_umask(mode));

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
    preempt_disable();

    int retval;
    int ready = 0;

    if (!fds && nfds > 0) {
        retval = -EINVAL;
        goto leave;
    }

    /* Reset timeout flag */
    running_thread->syscall_is_timeout = false;

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
    struct task_struct *task = current_task_info();

    for (int i = 0; i < nfds; i++) {
        fds[i].revents = 0;

        int fd = fds[i].fd;
        if (fd < 0)
            continue; /* Ignore */

        filp = fd_file(task, fd);
        if (!filp) {
            fds[i].revents |= POLLNVAL;
            ready++;
            continue;
        }
        /* A regular file is always ready, it has no event of its own to
         * wait for. Reporting otherwise leaves a reader of a redirected
         * standard input waiting for something that never arrives.
         */
        uint32_t ready_events = filp->f_events;
        if (filp->f_inode && S_ISREG(filp->f_inode->i_mode))
            ready_events |= POLLIN | POLLOUT;

        uint32_t events = ready_events & fds[i].events;
        if (events) {
            fds[i].revents |= events;
            ready++;
        }
    }

    if (ready > 0) {
        /* Return number of fds with events */
        retval = ready;
        goto leave;
    }

    /* No events is observed and no timeout is set, return immediately */
    if (timeout == 0) {
        retval = 0;
        goto leave;
    }

    /* Suspend current thread */
    prepare_to_wait(&poll_list, running_thread, THREAD_WAIT);

    /* Add current thread into the timeout monitoring list */
    if (timeout > 0)
        list_add_tail(&running_thread->timeout_list, &timeout_list);

    /* Record all files for polling */
    for (int i = 0; i < nfds; i++) {
        int fd = fds[i].fd;
        if (fd < 0)
            continue;

        filp = fd_file(task, fd);
        if (!filp) {
            fds[i].revents |= POLLNVAL;
            ready++;
            continue;
        }

        list_add_tail(&filp->list, &running_thread->poll_files_list);
    }

    /* Wait until the file event happens */
    schedule();

    /* clear list of poll files */
    INIT_LIST_HEAD(&running_thread->poll_files_list);

    /* Remove the thread from the polling list */
    if (timeout > 0)
        list_del(&running_thread->timeout_list);

    if (running_thread->syscall_is_timeout) {
        retval = 0;
        goto leave;
    }

    ready = 0;
    for (int i = 0; i < nfds; i++) {
        fds[i].revents = 0;

        int fd = fds[i].fd;
        if (fd < 0)
            continue; /* Ignore */

        filp = fd_file(task, fd);
        if (!filp) {
            fds[i].revents |= POLLNVAL;
            ready++;
            continue;
        }
        /* A regular file is always ready, it has no event of its own to
         * wait for. Reporting otherwise leaves a reader of a redirected
         * standard input waiting for something that never arrives.
         */
        uint32_t ready_events = filp->f_events;
        if (filp->f_inode && S_ISREG(filp->f_inode->i_mode))
            ready_events |= POLLIN | POLLOUT;

        uint32_t events = ready_events & fds[i].events;
        if (events) {
            fds[i].revents |= events;
            ready++;
        }
    }

    retval = ready;

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

    if (!newattr) {
        retval = -EINVAL;
        goto leave;
    }

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
    if (oldattr) {
        oldattr->mq_flags = curr_attr->mq_flags;
        oldattr->mq_maxmsg = curr_attr->mq_maxmsg;
        oldattr->mq_msgsize = curr_attr->mq_msgsize;
        oldattr->mq_curmsgs = __mq_len(mqd_table[mqdes].mq);
    }

    /* Save new mq_flags attribute */
    curr_attr->mq_flags = newattr->mq_flags & O_NONBLOCK;

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

    if (!name) {
        retval = -EFAULT;
        goto leave;
    }

    if (strlen(name) >= NAME_MAX) {
        retval = -ENAMETOOLONG;
        goto leave;
    }

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
        mqd_table[mqdes].attr.mq_flags = oflag & O_NONBLOCK;

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
    strncpy(new_mq->name, name, NAME_MAX - 1);
    new_mq->name[NAME_MAX - 1] = '\0';
    list_add_tail(&new_mq->list, &mqueue_list);

    /* Register new message queue descriptor */
    bitmap_set_bit(bitmap_mqds, mqdes);
    bitmap_set_bit(task->bitmap_mqds, mqdes);
    mqd_table[mqdes].mq = new_mq;
    mqd_table[mqdes].attr = *attr;
    mqd_table[mqdes].attr.mq_flags = oflag & O_NONBLOCK;

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
    preempt_disable();

    ssize_t retval;

    if (!msg_ptr) {
        retval = -EFAULT;
        goto leave;
    }

    /* Check if the message queue descriptor is invalid */
    struct task_struct *task = current_task_info();
    if (!bitmap_get_bit(bitmap_mqds, mqdes) ||
        !bitmap_get_bit(task->bitmap_mqds, mqdes)) {
        retval = -EBADF;
        goto leave;
    }

    /* Acquire the message queue */
    struct mqueue *mq = mqd_table[mqdes].mq;

    /* Check if msg_len exceeds maximum size */
    if (msg_len > mqd_table[mqdes].attr.mq_msgsize) {
        retval = -EMSGSIZE;
        goto leave;
    }

    /* Read message */
    while (1) {
        retval = mq_do_receive(mq, &mqd_table[mqdes].attr, msg_ptr, msg_len,
                               msg_prio);

        if (retval != -ERESTARTSYS)
            break;

        schedule();
    }

leave:
    preempt_enable();
    return retval;
}

static ssize_t sys_mq_timedreceive(mqd_t mqdes,
                                   char *msg_ptr,
                                   size_t msg_len,
                                   unsigned int *msg_prio,
                                   const struct timespec *abstime)
{
    preempt_disable();

    ssize_t retval;

    if (!msg_ptr || !timespec_valid(abstime)) {
        retval = -EINVAL;
        goto leave;
    }

    /* Check if the message queue descriptor is invalid */
    struct task_struct *task = current_task_info();
    if (!bitmap_get_bit(bitmap_mqds, mqdes) ||
        !bitmap_get_bit(task->bitmap_mqds, mqdes)) {
        retval = -EBADF;
        goto leave;
    }

    /* Acquire the message queue */
    struct mqueue *mq = mqd_table[mqdes].mq;

    /* Check if msg_len exceeds maximum size */
    if (msg_len > mqd_table[mqdes].attr.mq_msgsize) {
        retval = -EMSGSIZE;
        goto leave;
    }

    while (1) {
        retval = mq_do_receive(mq, &mqd_table[mqdes].attr, msg_ptr, msg_len,
                               msg_prio);

        if (retval != -ERESTARTSYS)
            break;

        struct timespec now;
        get_sys_time(&now);
        if (timespec_cmp(&now, abstime) >= 0) {
            retval = -ETIMEDOUT;
            break;
        }

        running_thread->syscall_is_timeout = false;
        running_thread->syscall_timeout = *abstime;
        list_add_tail(&running_thread->timeout_list, &timeout_list);

        schedule();

        list_del(&running_thread->timeout_list);

        if (running_thread->syscall_is_timeout) {
            retval = -ETIMEDOUT;
            break;
        }
    }

leave:
    preempt_enable();
    return retval;
}

static int sys_mq_send(mqd_t mqdes,
                       const char *msg_ptr,
                       size_t msg_len,
                       unsigned int msg_prio)
{
    preempt_disable();

    int retval;

    if (!msg_ptr) {
        retval = -EFAULT;
        goto leave;
    }

    /* Check if the message priority exceeds the max value */
    if (msg_prio > MQ_PRIO_MAX) {
        retval = -EINVAL;
        goto leave;
    }

    /* Check if the message queue descriptor is invalid */
    struct task_struct *task = current_task_info();
    if (!bitmap_get_bit(bitmap_mqds, mqdes) ||
        !bitmap_get_bit(task->bitmap_mqds, mqdes)) {
        retval = -EBADF;
        goto leave;
    }

    /* Acquire the message queue */
    struct mqueue *mq = mqd_table[mqdes].mq;

    /* Check if msg_len exceeds maximum size */
    if (msg_len > mqd_table[mqdes].attr.mq_msgsize) {
        retval = -EMSGSIZE;
        goto leave;
    }

    /* Send message */
    while (1) {
        retval =
            mq_do_send(mq, &mqd_table[mqdes].attr, msg_ptr, msg_len, msg_prio);

        if (retval != -ERESTARTSYS)
            break;

        schedule();
    }

leave:
    preempt_enable();
    return retval;
}

static int sys_mq_timedsend(mqd_t mqdes,
                            const char *msg_ptr,
                            size_t msg_len,
                            unsigned int msg_prio,
                            const struct timespec *abstime)
{
    preempt_disable();

    int retval;

    if (!msg_ptr || !timespec_valid(abstime)) {
        retval = -EINVAL;
        goto leave;
    }

    /* Check if the message priority exceeds the max value */
    if (msg_prio > MQ_PRIO_MAX) {
        retval = -EINVAL;
        goto leave;
    }

    /* Check if the message queue descriptor is invalid */
    struct task_struct *task = current_task_info();
    if (!bitmap_get_bit(bitmap_mqds, mqdes) ||
        !bitmap_get_bit(task->bitmap_mqds, mqdes)) {
        retval = -EBADF;
        goto leave;
    }

    /* Acquire the message queue */
    struct mqueue *mq = mqd_table[mqdes].mq;

    /* Check if msg_len exceeds maximum size */
    if (msg_len > mqd_table[mqdes].attr.mq_msgsize) {
        retval = -EMSGSIZE;
        goto leave;
    }

    while (1) {
        retval =
            mq_do_send(mq, &mqd_table[mqdes].attr, msg_ptr, msg_len, msg_prio);

        if (retval != -ERESTARTSYS)
            break;

        struct timespec now;
        get_sys_time(&now);
        if (timespec_cmp(&now, abstime) >= 0) {
            retval = -ETIMEDOUT;
            break;
        }

        running_thread->syscall_is_timeout = false;
        running_thread->syscall_timeout = *abstime;
        list_add_tail(&running_thread->timeout_list, &timeout_list);

        schedule();

        list_del(&running_thread->timeout_list);

        if (running_thread->syscall_is_timeout) {
            retval = -ETIMEDOUT;
            break;
        }
    }

leave:
    preempt_enable();
    return retval;
}

static int sys_pthread_create(pthread_t *pthread,
                              const pthread_attr_t *_attr,
                              void *(*start_routine)(void *),
                              void *arg)
{
    preempt_disable();

    if (!pthread || !start_routine) {
        preempt_enable();
        return -EINVAL;
    }

    struct thread_attr *attr = (struct thread_attr *) _attr;

    /* Use defualt attributes if user did not provide */
    struct thread_attr default_attr;
    if (attr == NULL) {
        pthread_attr_init((pthread_attr_t *) &default_attr);
        default_attr.inheritsched = PTHREAD_INHERIT_SCHED;
        attr = &default_attr;
    }

    /* A thread told to inherit takes the priority of the one creating it, and
     * whatever the attributes say about scheduling is passed over
     */
    struct thread_attr inherited_attr;
    if (attr->inheritsched == PTHREAD_INHERIT_SCHED) {
        inherited_attr = *attr;
        inherited_attr.schedparam.sched_priority = running_thread->priority;
        inherited_attr.schedpolicy = SCHED_RR;
        attr = &inherited_attr;
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
        list_add_tail(&thread->task_list, &current_task_info()->threads_list);

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
        /* The thread may have ended already, in which case its return value is
         * waiting to be handed over and this call has nothing to wait for
         */
        thread = acquire_ended_thread(tid);
        if (!thread) {
            /* Return error */
            retval = -ESRCH;
            goto leave;
        }

        if (pthread_retval)
            *pthread_retval = thread->retval;

        thread_reap(thread);

        /* Return success */
        retval = 0;
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

    list_add_tail(&running_thread->list, &thread->join_list);
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

    /* kthread can only be canceled by the kernel. What says a thread is one is
     * the flag it was made with, not its privilege: that says which mode the
     * thread is running in, and every thread runs in the kernel's while it is
     * inside a system call, this one included
     */
    if (thread->kernel_thread) {
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

    /* Nothing will ask a thread that has already ended for its return value
     * once it is detached, so it has no reason to stay
     */
    if (threads[tid].status == THREAD_TERMINATED)
        thread_reap(&threads[tid]);

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

    if (!param) {
        retval = -EINVAL;
        goto leave;
    }

    struct thread_info *thread = acquire_thread(tid);
    if (!thread) {
        /* Return error */
        retval = -ESRCH;
        goto leave;
    }

    /* kthread can only be configured by the kernel */
    if (thread->kernel_thread) {
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

    if (!param || !policy) {
        retval = -EINVAL;
        goto leave;
    }

    struct thread_info *thread = acquire_thread(tid);
    if (!thread) {
        /* Return error */
        retval = -ESRCH;
        goto leave;
    }

    /* kthread can only be set by the kernel */
    if (thread->kernel_thread) {
        /* Return error */
        retval = -EPERM;
        goto leave;
    }

    /* Return settings */
    *policy = SCHED_RR;
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
        if (thread->ret_siginfo) {
            thread->ret_siginfo->si_signo = signum;
            thread->ret_siginfo->si_code = 0;
            thread->ret_siginfo->si_value.sival_int = 0;
            thread->ret_siginfo = NULL;
        }
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
        enqueue_pending_signal(thread, (uint32_t) act->sa_sigaction, args);
    } else {
        uint32_t args[4] = {0};
        args[0] = (uint32_t) signum;
        enqueue_pending_signal(thread, (uint32_t) act->sa_handler, args);
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
    if (thread->kernel_thread) {
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

void thread_inherit_priority(struct mutex *mutex)
{
    if (mutex->protocol != PTHREAD_PRIO_INHERIT)
        return;

    preempt_disable();

    struct thread_info *onwer_thread = mutex->owner;

    /* Preserve original priority */
    if (!onwer_thread->priority_inherited) {
        running_thread->original_priority = running_thread->priority;
        onwer_thread->priority_inherited = true;
    }

    /* Priority Inheritance Protocol (PIP) */
    if (running_thread->priority > onwer_thread->priority) {
        /* Raise the priority of the owner thread */
        uint8_t old_priority = onwer_thread->priority;
        uint8_t new_priority = running_thread->priority;

        /* Move the owner thread from the ready list with lower priority to
         * the new list with raised priority  */
        struct list_head *curr, *next;
        list_for_each_safe (curr, next, &ready_list[old_priority]) {
            struct thread_info *thread =
                list_entry(curr, struct thread_info, list);
            if (thread == mutex->owner) {
                list_move_tail(&thread->list, &ready_list[new_priority]);
            }
        }

        /* Set new raised priority */
        onwer_thread->priority = new_priority;
    }

    preempt_enable();
}

void thread_reset_inherited_priority(struct mutex *mutex)
{
    preempt_disable();

    if (running_thread->priority_inherited) {
        /* Recover the original thread priority */
        running_thread->priority = running_thread->original_priority;
        running_thread->priority_inherited = false;
    }

    preempt_enable();
}

static int sys_pthread_mutex_unlock(pthread_mutex_t *mutex)
{
    return mutex_unlock((struct mutex *) mutex);
}

static int sys_pthread_mutex_lock(pthread_mutex_t *mutex)
{
    return mutex_lock((struct mutex *) mutex);
}

static int sys_pthread_mutex_trylock(pthread_mutex_t *mutex)
{
    return mutex_trylock((struct mutex *) mutex);
}

static int sys_pthread_mutex_timedlock(pthread_mutex_t *mutex,
                                       const struct timespec *abstime)
{
    if (!mutex || !timespec_valid(abstime))
        return -EINVAL;

    struct mutex *mtx = (struct mutex *) mutex;

    while (1) {
        int retval = mutex_trylock(mtx);

        if (retval == 0) {
            thread_reset_inherited_priority(mtx);
            return 0;
        }

        if (retval != -EBUSY)
            return retval;

        thread_inherit_priority(mtx);

        struct timespec now;
        get_sys_time(&now);
        if (timespec_cmp(&now, abstime) >= 0)
            return -ETIMEDOUT;

        preempt_disable();
        running_thread->syscall_is_timeout = false;
        running_thread->syscall_timeout = *abstime;
        list_add_tail(&running_thread->timeout_list, &timeout_list);
        preempt_enable();

        schedule();

        preempt_disable();
        list_del(&running_thread->timeout_list);
        bool timeout = running_thread->syscall_is_timeout;
        preempt_enable();

        if (timeout)
            return -ETIMEDOUT;
    }
}

/* A condition variable written down with PTHREAD_COND_INITIALIZER is all zeros
 * in the same way a mutex is, and is made into a list here for the same reason
 */
static struct list_head *cond_wait_list(pthread_cond_t *cond)
{
    struct cond *_cond = (struct cond *) cond;

    if (!_cond->task_wait_list.next || !_cond->task_wait_list.prev)
        INIT_LIST_HEAD(&_cond->task_wait_list);

    return &_cond->task_wait_list;
}

static int sys_pthread_cond_signal(pthread_cond_t *cond)
{
    /* Wake up a thread from the wait list */
    wake_up(cond_wait_list(cond));

    /* Return success */
    return 0;
}

static int sys_pthread_cond_broadcast(pthread_cond_t *cond)
{
    /* Wake up all threads from the wait list */
    wake_up_all(cond_wait_list(cond));

    /* Return success */
    return 0;
}

static int sys_pthread_cond_wait(pthread_cond_t *cond, pthread_mutex_t *mutex)
{
    preempt_disable();

    if (!cond || !mutex) {
        preempt_enable();
        return -EINVAL;
    }

    int retval = mutex_unlock((struct mutex *) mutex);
    if (retval != 0) {
        preempt_enable();
        return retval;
    }

    /* Enqueue current thread into the wait list */
    prepare_to_wait(cond_wait_list(cond), running_thread, THREAD_WAIT);

    preempt_enable();

    /* Block until signaled */
    schedule();

    /* Reacquire the mutex before returning */
    return mutex_lock((struct mutex *) mutex);
}

static int sys_pthread_cond_timedwait(pthread_cond_t *cond,
                                      pthread_mutex_t *mutex,
                                      const struct timespec *abstime)
{
    if (!cond || !mutex || !timespec_valid(abstime))
        return -EINVAL;

    struct timespec now;
    get_sys_time(&now);
    if (timespec_cmp(&now, abstime) >= 0)
        return -ETIMEDOUT;

    int retval = mutex_unlock((struct mutex *) mutex);
    if (retval != 0)
        return retval;

    preempt_disable();
    running_thread->syscall_is_timeout = false;
    running_thread->syscall_timeout = *abstime;
    list_add_tail(&running_thread->timeout_list, &timeout_list);

    prepare_to_wait(cond_wait_list(cond), running_thread, THREAD_WAIT);
    preempt_enable();

    schedule();

    preempt_disable();
    list_del(&running_thread->timeout_list);
    bool timeout = running_thread->syscall_is_timeout;
    preempt_enable();

    retval = mutex_lock((struct mutex *) mutex);
    if (retval != 0)
        return retval;

    if (timeout)
        return -ETIMEDOUT;

    return 0;
}

/* The routine a once control names is written by the caller and has to run in
 * user space, so pthread_once() is in two halves with the call between them.
 * The first says whether this thread is the one to run it, waiting if another
 * thread got there first; the second says it has been run.
 */
static int sys_pthread_once_begin(pthread_once_t *_once_control)
{
    preempt_disable();

    struct thread_once *once = (struct thread_once *) _once_control;

    /* A once control written down with PTHREAD_ONCE_INIT is all zeros, and a
     * list of zeros is not yet a list
     */
    if (!once->wait_list.next || !once->wait_list.prev)
        INIT_LIST_HEAD(&once->wait_list);

    if (once->finished) {
        preempt_enable();
        return 1;
    }

    if (!once->running) {
        once->running = true;
        preempt_enable();
        return 0;
    }

    /* Another thread is running the routine. Wait for it to say it is done,
     * so that this call does not return before the routine has been run
     */
    prepare_to_wait(&once->wait_list, running_thread, THREAD_WAIT);
    preempt_enable();
    schedule();

    return 1;
}

static int sys_pthread_once_end(pthread_once_t *_once_control)
{
    preempt_disable();

    struct thread_once *once = (struct thread_once *) _once_control;

    once->running = false;
    once->finished = true;

    if (!once->wait_list.next || !once->wait_list.prev)
        INIT_LIST_HEAD(&once->wait_list);

    wake_up_all(&once->wait_list);

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

static int sys_sem_timedwait(sem_t *sem, const struct timespec *abstime)
{
    if (!sem || !timespec_valid(abstime))
        return -EINVAL;

    preempt_disable();

    struct semaphore *ksem = (struct semaphore *) sem;

    if (ksem->count > 0) {
        ksem->count--;
        preempt_enable();
        return 0;
    }

    struct timespec now;
    get_sys_time(&now);
    if (timespec_cmp(&now, abstime) >= 0) {
        preempt_enable();
        return -ETIMEDOUT;
    }

    running_thread->syscall_is_timeout = false;
    running_thread->syscall_timeout = *abstime;
    list_add_tail(&running_thread->timeout_list, &timeout_list);

    while (ksem->count <= 0) {
        prepare_to_wait(&ksem->wait_list, running_thread, THREAD_WAIT);
        schedule();

        if (running_thread->syscall_is_timeout)
            break;
    }

    list_del(&running_thread->timeout_list);

    if (running_thread->syscall_is_timeout) {
        preempt_enable();
        return -ETIMEDOUT;
    }

    ksem->count--;
    preempt_enable();
    return 0;
}

static int sys_sem_getvalue(sem_t *sem, int *sval)
{
    preempt_disable();

    if (!sem || !sval) {
        preempt_enable();
        return -EINVAL;
    }

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

    /* Preserve old signal action if requested */
    if (oldact && sig_entry)
        *oldact = *sig_entry;

    /* If act is NULL, only query old action */
    if (!act) {
        retval = 0;
        goto leave;
    }

    /* Has the signal action already been registered on the table? */
    if (sig_entry) {
        /* Replace old signal action */
        *sig_entry = *act;
    } else {
        /* Allocate memory for new action */
        struct sigaction *new_act = kmalloc(sizeof(*new_act));

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

    if (!set || !sig) {
        retval = -EINVAL;
        goto leave;
    }

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
    running_thread->ret_siginfo = NULL;
    running_thread->wait_for_signal = true;

    /* Enqueue the thread into the signal waiting list */
    prepare_to_wait(&suspend_list, running_thread, THREAD_WAIT);

    /* Return success */
    retval = 0;

leave:
    preempt_enable();
    return retval;
}

static int sys_sigwaitinfo(const sigset_t *set, siginfo_t *info)
{
    preempt_disable();

    int retval;

    if (!set) {
        retval = -EINVAL;
        goto leave;
    }

    sigset_t invalid_mask =
        ~(sig2bit(SIGUSR1) | sig2bit(SIGUSR2) | sig2bit(SIGPOLL) |
          sig2bit(SIGSTOP) | sig2bit(SIGCONT) | sig2bit(SIGKILL));

    if (*set & invalid_mask) {
        retval = -EINVAL;
        goto leave;
    }

    int sig = 0;

    running_thread->sig_wait_set = *set;
    running_thread->ret_sig = &sig;
    running_thread->ret_siginfo = info;
    running_thread->wait_for_signal = true;

    prepare_to_wait(&suspend_list, running_thread, THREAD_WAIT);

    preempt_enable();
    schedule();
    preempt_disable();

    running_thread->ret_siginfo = NULL;
    retval = sig;

leave:
    preempt_enable();
    return retval;
}

static int sys_sigtimedwait(const sigset_t *set,
                            siginfo_t *info,
                            const struct timespec *timeout)
{
    preempt_disable();

    int retval;

    if (!set) {
        retval = -EINVAL;
        goto leave;
    }

    sigset_t invalid_mask =
        ~(sig2bit(SIGUSR1) | sig2bit(SIGUSR2) | sig2bit(SIGPOLL) |
          sig2bit(SIGSTOP) | sig2bit(SIGCONT) | sig2bit(SIGKILL));

    if (*set & invalid_mask) {
        retval = -EINVAL;
        goto leave;
    }

    struct timespec abstime;
    if (timeout) {
        if (!timespec_valid(timeout)) {
            retval = -EINVAL;
            goto leave;
        }
        get_sys_time(&abstime);
        time_add(&abstime, timeout->tv_sec, timeout->tv_nsec);
    }

    int sig = 0;

    running_thread->sig_wait_set = *set;
    running_thread->ret_sig = &sig;
    running_thread->ret_siginfo = info;
    running_thread->wait_for_signal = true;

    if (timeout) {
        running_thread->syscall_is_timeout = false;
        running_thread->syscall_timeout = abstime;
        list_add_tail(&running_thread->timeout_list, &timeout_list);
    }

    prepare_to_wait(&suspend_list, running_thread, THREAD_WAIT);

    preempt_enable();
    schedule();
    preempt_disable();

    if (timeout)
        list_del(&running_thread->timeout_list);

    if (timeout && running_thread->syscall_is_timeout) {
        running_thread->ret_siginfo = NULL;
        retval = -ETIMEDOUT;
        goto leave;
    }

    running_thread->ret_siginfo = NULL;
    retval = sig;

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

    if (!tp) {
        retval = -EINVAL;
        goto leave;
    }

    if (clockid == CLOCK_MONOTONIC) {
        get_sys_time(tp);
    } else if (clockid == CLOCK_REALTIME) {
        get_realtime(tp);
    } else {
        /* Return error */
        retval = -EINVAL;
        goto leave;
    }

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

    if (!tp) {
        retval = -EINVAL;
        goto leave;
    }

    if (clockid == CLOCK_MONOTONIC) {
        set_sys_time(tp);
    } else if (clockid == CLOCK_REALTIME) {
        set_realtime(tp);
    } else {
        /* Return error */
        retval = -EINVAL;
        goto leave;
    }

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

    if (!sevp) {
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
    list_add_tail(&new_tm->g_list, &timers_list);
    list_add_tail(&new_tm->list, &running_thread->timers_list);

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
    if (!new_value) {
        retval = -EINVAL;
        goto leave;
    }

    if (new_value->it_value.tv_sec < 0 || new_value->it_value.tv_nsec < 0 ||
        new_value->it_value.tv_nsec > 999999999 ||
        new_value->it_interval.tv_sec < 0 ||
        new_value->it_interval.tv_nsec < 0 ||
        new_value->it_interval.tv_nsec > 999999999) {
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

    if (!curr_value) {
        retval = -EINVAL;
        goto leave;
    }

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

static void *sys_memalign(size_t alignment, size_t size)
{
    preempt_disable();

    void *ptr = size ? __memalign(alignment, size) : NULL;

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

static void *sys_realloc(void *ptr, size_t size)
{
    preempt_disable();

    /* Resize the memory. The move of the contents happens under the same
     * lock as the allocation and the release, so no other thread sees the
     * block in two places at once
     */
    void *new_ptr = __realloc(ptr, size);

    preempt_enable();
    return new_ptr;
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
            enqueue_pending_signal(timer->thread, (uint32_t) notify_func, args);
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
        if (tp.tv_sec > thread->syscall_timeout.tv_sec ||
            (tp.tv_sec == thread->syscall_timeout.tv_sec &&
             tp.tv_nsec >= thread->syscall_timeout.tv_nsec)) {
            thread->syscall_is_timeout = true;
            finish_wait(thread);
        }
    }
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

void system_ticks_update(void)
{
    __preempt_disable();

    system_timer_update();
    threads_ticks_update();
    timers_update();
    syscall_timeout_update();

    set_need_resched();

    __preempt_enable();
}

static void syscall_return_event_handler(void)
{
    running_thread->stack_top =
        (unsigned long *) running_thread->syscall_stack_top;
    running_thread->privilege =
        running_thread->kernel_thread ? KERNEL_THREAD : USER_THREAD;
    running_thread->syscall_mode = false;

    /* Rescheduling if current thread relinquished the CPU */
    if (running_thread->status != THREAD_READY)
        set_need_resched();
}

static inline void signal_cleanup_event_handler(void)
{
    running_thread->stack_top =
        (unsigned long *) running_thread->stack_top_preserved;
    running_thread->stack_top_preserved = (unsigned long) NULL;
}

static void thread_return_event_handler(void)
{
    /* A thread that returns leaves its answer in the first register, and the
     * exception frame it is read out of is the same one a system call is read
     * from. Nothing has filled in the argument pointers on this path, though:
     * only a call listed in the system call table goes past the events above
     */
    get_syscall_args(running_thread->stack_top, running_thread->syscall_args);

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

    /* Rescheduling as current thread is terminated */
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

void set_syscall_flag(void)
{
    syscall_flag = true;
}

void reset_syscall_flag(void)
{
    syscall_flag = false;
}

bool get_syscall_flag(void)
{
    return syscall_flag;
}

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
            list_move_tail(curr, &ready_list[thread->priority]);
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
    /* Request rescheduling */
    preempt_disable();
    set_need_resched();
    preempt_enable();

    /* Jump back to the kernel loop */
    jump_to_kernel();
}

static void print_platform_info(void)
{
#ifndef BUILD_QEMU
    /* Clear screen */
    char *cls_str = "\x1b[H\x1b[2J";
    console_write("\x1b[H\x1b[2J", strlen(cls_str));
#endif
    printk("Tenok (built time: %s %s)", __TIME__, __DATE__);
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
    print_platform_info();
    __board_init();
    rom_dev_init();
    null_dev_init();
    link_stdin_dev(STDIN_PATH);
    link_stdout_dev(STDOUT_PATH);
    link_stderr_dev(STDERR_PATH);
    printkd_start();

    /* Mount rom file system */
    mount("/dev/rom", "/");

    /* Wait until the boot message is printed */
    while (!printk_all_flushed())
        sched_yield();

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
    param.sched_priority = KTHREAD_PRI_MAX - 1;
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
        /* Syscall request */
        if (get_syscall_flag()) {
            reset_syscall_flag();
            syscall_handler();
        }

        /* Rescheduling request */
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
