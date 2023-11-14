/**
 * @file
 */
#ifndef __TASK_H__
#define __TASK_H__

#include <stdint.h>

typedef void (*task_func_t)(void);
typedef void (*thread_func_t)(void);

struct task_hook {
    task_func_t task_func;
    int priority;
    int stacksize;
};

/**
 * @brief  Register a user task to be launch at the bootup
 *         stage
 * @param  task_func: The task function to launch.
 * @param  priority: The task priority to set.
 * @param  stacksize: The stack size to allocate for the task.
 * @retval None
 */
#define HOOK_USER_TASK(_task_func, _priority, _stacksize) \
    static struct task_hook _##_task_func                 \
        __attribute__((section(".tasks"), used)) = {      \
            .task_func = _task_func,                      \
            .priority = _priority,                        \
            .stacksize = _stacksize,                      \
    }

/**
 * @brief  Create new task
 * @param  task_func: Task function to run.
 * @param  priority: Priority of the task.
 * @param  stack_size: Stack size of the task.
 * @retval int: The function return positive task pid number if
 *              success; otherwise it returns a negative error
 *              number.
 */
int task_create(task_func_t task_func, uint8_t priority, int stack_size);

#endif
