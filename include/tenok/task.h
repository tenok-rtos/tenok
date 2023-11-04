/**
 * @file
 */
#ifndef __TASK_H__
#define __TASK_H__

#include <kernel/kernel.h>

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
