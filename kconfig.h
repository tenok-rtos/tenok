#ifndef __KCONFIG_H__
#define __KCONFIG_H__

#define OS_TICK_FREQ       1000 //Hz

#define TASK_STACK_SIZE    512  //words
#define TASK_NUM_MAX       10
#define TASK_MAX_PRIORITY  5
#define TASK_NAME_LEN_MAX  50

#define MEM_POOL_SIZE      4096

#define FILE_CNT_LIMIT     20
#define FILE_NAME_LEN_MAX  30

#define PIPE_DEPTH         50

#define SYSCALL_INTR_PRI   5    /* isr priority higher than this value *
                                 * will not be disabled                */

/* file system */
#define FILE_SYSTEM_FD     0

#define PATH_LEN_MAX       128

#define MOUNT_CNT_MAX      5
#define INODE_CNT_MAX      100

#define ROOTFS_BLK_SIZE    128  //bytes
#define ROOTFS_BLK_CNT     100

#endif
