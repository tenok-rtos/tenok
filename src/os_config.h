#ifndef __CONFIG_RTOS_H__
#define __CONFIG_RTOS_H__

#define OS_TICK_FREQUENCY        (1000)

#define TASK_NUM_MAX             (10)

#define TASK_NAME_LEN_MAX        (100) //bytes
#define TASK_STACK_SIZE          (512) //words

#define OS_MAX_PRIORITY          (16)
#define SYSCALL_INT_PRIORITY_MAX (5)

#endif
