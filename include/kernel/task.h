/**
 * @file
 */
#ifndef __KERNEL_TASK_H__
#define __KERNEL_TASK_H__

#include <kernel/kernel.h>

struct task_hook {
    task_func_t task_func;
    int priority;
    int stacksize;
};

/**
 * @brief  Register a user task to be launch at the bootup
 *         stage.
 * @param  task_func: The task function to launch.
 * @param  priority: The task priority to set.
 * @param  stacksize: The stack size to allocate for the task.
 * @retval None
 */
#define HOOK_USER_TASK(_task_func, _priority, _stacksize) \
    static struct task_hook _ ## _task_func \
        __attribute__((section(".tasks"), used)) = { \
        .task_func = _task_func, \
        .priority = _priority, \
        .stacksize = _stacksize \
    }

#endif
