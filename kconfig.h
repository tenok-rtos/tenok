/* clang-format off */
#ifndef __KCONFIG_H__
#define __KCONFIG_H__

/* Scheduler */
#define OS_TICK_FREQ         1000  /* Hz */

/* Page memory */
#define PAGE_SIZE_32K        0
#define PAGE_SIZE_64K        1
#define PAGE_SIZE_SELECT     PAGE_SIZE_64K

/* The minimum stack size recommended for task and thread creation. since *
 * every thread consumes a part of it own stack as overhead, stack size   *
 * lower than this value may cause the system to be unstable.             */
#define STACK_SIZE_MIN       1024  /* Bytes */

/* Thread */
#define THREAD_PRIORITY_MAX  8     /* Max priority for user threads */
#define THREAD_NAME_MAX      50

/* Message queue and pipe */
#define MQUEUE_CNT_MAX       50

/* Thread anonymous pipe size. Ff the depth is too small, the file server * 
 * may not handle the request properly.                                   */
#define PIPE_DEPTH           100   /* Bytes */

/* Signals */
#define SIGNAL_QUEUE_SIZE    5

/* Standard I/O */
#define STDIN_DEV_PATH       "/dev/serial0"
#define STDOUT_DEV_PATH      "/dev/serial0"
#define STDERR_DEV_PATH      "/dev/serial0"

/* File system */
#define FILE_CNT_MAX         100   /* The maximum number of the files can be *
                                    * created, which includes directories.   */
#define FILE_NAME_LEN_MAX    30
#define OPEN_MAX             100   /* The maximum number of files a task can *
                                    * open                                   */

#define PATH_LEN_MAX         128

#define MOUNT_CNT_MAX        5
#define INODE_CNT_MAX        100

#define FS_BLK_SIZE          128   /* Bytes. Block size of the file system */
#define FS_BLK_CNT           100   /* Block count of the file system       */

#define FS_BITMAP_INODE      50    /* Word */
#define FS_BITMAP_BLK        50    /* Word */

/* Shell */
#define SHELL_HISTORY_MAX    20
#define SHELL_CMD_LEN_MAX    50
#define SHELL_PROMPT_LEN_MAX 50

#define PRINT_SIZE_MAX       100

#define USE_TENOK_PRINTF     0

#endif
/* clang-format on */
