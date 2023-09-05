#ifndef __TASK_H__
#define __TASK_H__

#include "kernel.h"

#define HOOK_USER_TASK(task_func) \
    static task_func_t _  ## task_func \
        __attribute__((section(".tasks"), used)) = task_func

#endif
