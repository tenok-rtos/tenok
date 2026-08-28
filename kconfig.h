/**
 * @file
 *
 * What the kernel calls each thing the build was configured with. The values
 * themselves are chosen in Kconfig and written out by the build; this says
 * only which name the code knows each one by, so that the code reads the way
 * it always has.
 */
#ifndef __KCONFIG_H__
#define __KCONFIG_H__

#include <generated/autoconf.h>

/* Scheduler */
#define OS_TICK_FREQ CONFIG_OS_TICK_FREQ /* Hz */

/* Page allocator size */
#define PAGE_SIZE_32K 0 /* Use 32 KiB */
#define PAGE_SIZE_64K 1 /* Use 64 KiB */

#ifdef CONFIG_PAGE_SIZE_32K_SELECT
#define PAGE_SIZE_SELECT PAGE_SIZE_32K
#else
#define PAGE_SIZE_SELECT PAGE_SIZE_64K
#endif

/* Min stack size recommended for task and thread */
#define STACK_SIZE_MIN CONFIG_STACK_SIZE_MIN /* Bytes */

/* Daemons */
#define INIT_STACK_SIZE CONFIG_INIT_STACK_SIZE
#define IDLE_STACK_SIZE CONFIG_IDLE_STACK_SIZE
#define SOFTIRQD_STACK_SIZE CONFIG_SOFTIRQD_STACK_SIZE
#define FILESYSD_STACK_SIZE CONFIG_FILESYSD_STACK_SIZE
#define PRINTKD_STACK_SIZE CONFIG_PRINTKD_STACK_SIZE

/* Task */
#define TASK_MAX CONFIG_TASK_MAX

/* Thread */
#define THREAD_PRIORITY_MAX CONFIG_THREAD_PRIORITY_MAX
#define THREAD_NAME_MAX CONFIG_THREAD_NAME_MAX
#define THREAD_MAX CONFIG_THREAD_MAX

/* Thread-local storage */
#define _PTHREAD_KEYS_MAX CONFIG_PTHREAD_KEYS_MAX
#define _PTHREAD_DESTRUCTOR_ITERATIONS CONFIG_PTHREAD_DESTRUCTOR_ITERATIONS

/* Message queue and pipe */
#define MQUEUE_MAX CONFIG_MQUEUE_MAX
#define _MQ_PRIO_MAX CONFIG_MQ_PRIO_MAX
#define THREAD_PIPE_BUF CONFIG_THREAD_PIPE_BUF /* Bytes */
#define _PIPE_BUF CONFIG_PIPE_BUF              /* Bytes */

/* Signals */
#define SIGNAL_QUEUE_SIZE CONFIG_SIGNAL_QUEUE_SIZE

/* Standard I/O (Use /dev/null if not implemented) */
#define STDIN_PATH CONFIG_STDIN_PATH
#define STDOUT_PATH CONFIG_STDOUT_PATH
#define STDERR_PATH CONFIG_STDERR_PATH

#define PRINT_SIZE_MAX CONFIG_PRINT_SIZE_MAX /* Buffer of printf and printk */

#ifdef CONFIG_USE_TENOK_PRINTF
#define USE_TENOK_PRINTF 1
#else
#define USE_TENOK_PRINTF 0
#endif

/* File system */
#define _NAME_MAX CONFIG_NAME_MAX
#define _PATH_MAX CONFIG_PATH_MAX
#define _FILE_OPEN_MAX CONFIG_FILE_OPEN_MAX
#define FILE_MAX CONFIG_FILE_MAX
#define MOUNT_MAX CONFIG_MOUNT_MAX
#define INODE_MAX CONFIG_INODE_MAX
#define FS_BLK_SIZE CONFIG_FS_BLK_SIZE
#define FS_BLK_CNT CONFIG_FS_BLK_CNT

/* What the system holds besides what the configuration names */
#define STD_STREAM_CNT 3 /* stdin, stdout and stderr */

/* A task holds the three streams besides the files it opens */
#define _OPEN_MAX (STD_STREAM_CNT + _FILE_OPEN_MAX)

/* Shell */
#define _LINE_MAX CONFIG_LINE_MAX
#define SHELL_HISTORY_MAX CONFIG_SHELL_HISTORY_MAX

#endif
