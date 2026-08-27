#ifndef __KCONFIG_H__
#define __KCONFIG_H__

/* Scheduler */
#define OS_TICK_FREQ 4000 /* Hz */

#ifdef BUILD_QEMU
#undef OS_TICK_FREQ
#define OS_TICK_FREQ 100 /* Hz */
#endif

/* Page allocator size */
#define PAGE_SIZE_32K 0 /* Use 32 KiB */
#define PAGE_SIZE_64K 1 /* Use 64 KiB */
#define PAGE_SIZE_SELECT PAGE_SIZE_64K

/* Min stack size recommended for task and thread */
#define STACK_SIZE_MIN 1024 /* Bytes */

/* Daemons */
#define INIT_STACK_SIZE 4096
#define IDLE_STACK_SIZE 1024
#define SOFTIRQD_STACK_SIZE 2048
#define FILESYSD_STACK_SIZE 2048
#define PRINTKD_STACK_SIZE 1024

/* Task */
#define TASK_MAX 32 /* Max number of tasks in the system */

/* Thread */
#define THREAD_PRIORITY_MAX 8 /* Max priority of user threads */
#define THREAD_NAME_MAX 50    /* Max length of thread names */
#define THREAD_MAX 32         /* Max number of threads in the system */

/* Thread-local storage. A key names a slot that every thread has one of, so
 * the count is paid for by every thread whether it uses one or not
 */
#define _PTHREAD_KEYS_MAX 8

/* How many times over a destructor that leaves a new value behind is called
 * again before the system gives up on it
 */
#define _PTHREAD_DESTRUCTOR_ITERATIONS 4

/* Message queue and pipe */
#define MQUEUE_MAX 16  /* Max number of message queue can be allocated */
#define _MQ_PRIO_MAX 5 /* Max message queue priority number */

/* The reply pipe every thread is given, which the file system daemon answers
 * through. Note that if the size is too small, the daemon may not work
 * properly. Every thread pays for this one, so it is kept small.
 */
#define THREAD_PIPE_BUF 100 /* Bytes */

/* How much a pipe holds, which POSIX also makes the largest write that reaches
 * it whole. Only a live pipe pays for this one.
 */
#define _PIPE_BUF 512 /* Bytes */

/* Signals */
#define SIGNAL_QUEUE_SIZE 5

/* Standard I/O (Use /dev/null if not implemented) */
#define STDIN_PATH "/dev/console"
#define STDOUT_PATH "/dev/console"
#define STDERR_PATH "/dev/console"

#define PRINT_SIZE_MAX 100 /* Buffer size of the printf and printk */

#define USE_TENOK_PRINTF 1 /* 1: Use Tenok printf, 0: Use NewlibC printf */

/* File system */
#define _NAME_MAX 30       /* Max length of files in bytes */
#define _PATH_MAX 128      /* Max length of pathname in bytes */
#define STD_STREAM_CNT 3   /* stdin, stdout and stderr */
#define _FILE_OPEN_MAX 100 /* Max number of files a task can open */

/* A task holds the three streams besides the files it opens */
#define _OPEN_MAX (STD_STREAM_CNT + _FILE_OPEN_MAX)
#define FILE_MAX 100    /* Max number of the files can be created */
#define MOUNT_MAX 5     /* Max number of storages can be mounted */
#define INODE_MAX 100   /* Max number of the inode can have */
#define FS_BLK_SIZE 128 /* Block size of the file system in bytes */
#define FS_BLK_CNT 100  /* Block number of the file system */

/* Shell */
#define _LINE_MAX 128
#define SHELL_HISTORY_MAX 10

#endif
