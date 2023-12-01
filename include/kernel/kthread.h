/**
 * @file
 */
#ifndef __KTHREAD_H__
#define __KTHREAD_H__

#include <kernel/task.h>

/**
 * @brief  Create new kernel thread
 * @param  task_func: Task function to run.
 * @param  priority: Priority of the kernel thread.
 * @param  stack_size: Stack size of the kernel thread.
 * @retval int: The function returns positive task PID on success;
 *         otherwise it returns a negative error number.
 */
int kthread_create(task_func_t task_func, uint8_t priority, int stack_size);

#endif
