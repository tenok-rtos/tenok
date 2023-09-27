#ifndef __TASK_H__
#define __TASK_H__

#include <kernel/kernel.h>

int task_create(task_func_t task_func, uint8_t priority, int stack_size);

#endif
