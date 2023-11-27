#ifndef __KCONFIG_H__
#define __KCONFIG_H__

/* Scheduler */
#define OS_TICK_FREQ 1000 /* Hz */

/* Page allocator size */
#define PAGE_SIZE_32K 0 /* Use 32 KiB */
#define PAGE_SIZE_64K 1 /* Use 64 KiB */
#define PAGE_SIZE_SELECT PAGE_SIZE_64K

/* Min stack size recommended for task and thread */
#define STACK_SIZE_MIN 1024 /* Bytes */

/* Daemons */
#define IDLE_STACK_SIZE 2048
#define SOFTIRQD_STACK_SIZE 2048
#define FILESYSD_STACK_SIZE 2048
#define PRINTKD_STACK_SIZE 1024

/* Task */
#define TASK_MAX 64 /* Max number of tasks in the system */

/* Thread */
#define THREAD_PRIORITY_MAX 8 /* Max priority of user threads */
#define THREAD_NAME_MAX 50    /* Max length of thread names */
#define THREAD_MAX 64         /* Max number of threads in the system */

/* Message queue and pipe */
#define MQUEUE_MAX 50  /* Max number of message queue can be allocated */
#define _MQ_PRIO_MAX 5 /* Max message queue priority number */

/* Pipe size. Note that if the size is too small, the file system daemon *
 * may not work properly                                                 */
#define _PIPE_BUF 100 /* Bytes */

/* Signals */
#define SIGNAL_QUEUE_SIZE 5

/* Standard I/O (Use /dev/null if not implemented) */
#define STDIN_DEV_PATH "/dev/serial0"
#define STDOUT_DEV_PATH "/dev/serial0"
#define STDERR_DEV_PATH "/dev/serial0"

#define PRINT_SIZE_MAX 100 /* Buffer size of the printf and printk */

#define USE_TENOK_PRINTF 0 /* 1: Use Tenok printf, 0: Use NewlibC printf */

/* File system */
#define _NAME_MAX 30    /* Max length of files in bytes */
#define _PATH_MAX 128   /* Max length of pathname in bytes */
#define _OPEN_MAX 100   /* Max number of files a task can open */
#define FILE_MAX 100    /* Max number of the files can be created */
#define MOUNT_MAX 5     /* Max number of storages can be mounted */
#define INODE_MAX 100   /* Max number of the inode can have */
#define FS_BLK_SIZE 128 /* Block size of the file system in bytes */
#define FS_BLK_CNT 100  /* Block number of the file system */

/* Shell */
#define _LINE_MAX 50
#define SHELL_HISTORY_MAX 20

#endif
