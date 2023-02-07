#ifndef __KCONFIG_H__
#define __KCONFIG_H__

#define OS_TICK_FREQUENCY     (1000) //Hz

#define TASK_STACK_SIZE       (512)  //words
#define TASK_NUM_MAX          (10)
#define TASK_MAX_PRIORITY     (5)
#define TASK_NAME_LEN_MAX     (50)

#define MEM_POOL_SIZE         (2048)

#define FILE_CNT_LIMIT        (20)

#define PIPE_DEPTH            (50)


#define SYSCALL_INTR_PRIORITY (5)

#endif
