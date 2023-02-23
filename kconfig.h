#ifndef __KCONFIG_H__
#define __KCONFIG_H__

#define OS_TICK_FREQ         1000 //Hz

#define TASK_STACK_SIZE      1024 //words
#define TASK_NUM_MAX         10
#define TASK_MAX_PRIORITY    5
#define TASK_NAME_LEN_MAX    50

#define MEM_POOL_SIZE        4096

#define SYSCALL_INTR_PRI     5    /* isr priority higher than this value *
                                   * will not be disabled                */

/* message queue */
#define MQUEUE_MAX_CNT       50

/* file system */
#define FILE_SYSTEM_FD       1    /* file system program is assumed to be the *
                                   * second task to be launched               */

#define PIPE_DEPTH           100  /* notice that if the path depth is too shallow, *
                                   * the file operation request will failed        */

#define FILE_CNT_LIMIT       20
#define FILE_NAME_LEN_MAX    30

#define PATH_LEN_MAX         128

#define MOUNT_CNT_MAX        5
#define INODE_CNT_MAX        100

#define ROOTFS_BLK_SIZE      128  //bytes
#define ROOTFS_BLK_CNT       100

/* shell */
#define SHELL_HISTORY_MAX    20
#define SHELL_CMD_LEN_MAX    50
#define SHELL_PROMPT_LEN_MAX 50

#define PRINT_SIZE_MAX       400

#endif
