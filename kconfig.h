#ifndef __KCONFIG_H__
#define __KCONFIG_H__

/* page memory */
#define PAGE_SIZE_32K        0
#define PAGE_SIZE_64K        1
#define PAGE_SIZE_SELECT     PAGE_SIZE_64K

/* scheduler */
#define OS_TICK_FREQ         1000  /* Hz */

/* thread */
#define THREAD_PRIORITY_MAX  5
#define THREAD_NAME_MAX      50

/* message queue and pipe*/
#define MQUEUE_CNT_MAX       50

#define PIPE_DEPTH           100   /* notice that if the path depth is too shallow, *
                                    * the file operation request will failed        */

/* file system */
#define FILE_SYSTEM_FD       1

#define FILE_CNT_MAX         100   /* maximal size of the file can be created, *
                                    * including the directory                  */
#define FILE_NAME_LEN_MAX    30

#define FILE_DESC_CNT_MAX    20    /* numbers of file descriptors that one *
                                    * task can have                        */

#define PATH_LEN_MAX         128

#define MOUNT_CNT_MAX        5
#define INODE_CNT_MAX        100

#define FS_BLK_SIZE          128   /* bytes, block size of the file system */
#define FS_BLK_CNT           100   /* block count of the file system       */

#define FS_BITMAP_INODE      50    /* word */
#define FS_BITMAP_BLK        50    /* word */

/* shell */
#define SHELL_HISTORY_MAX    20
#define SHELL_CMD_LEN_MAX    50
#define SHELL_PROMPT_LEN_MAX 50

#define PRINT_SIZE_MAX       100

#define USE_TENOK_PRINTF     0

#endif
