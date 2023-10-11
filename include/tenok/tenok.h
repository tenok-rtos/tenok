#ifndef __TENOK_H__
#define __TENOK_H__

#include <stdint.h>
#include <stdbool.h>
#include <sys/types.h>

#include <kernel/kernel.h>

#include "kconfig.h"

struct procstat_info {
    int    pid;
    int    priority;
    int    status;
    bool   privileged;
    size_t stack_usage;
    size_t stack_size;
    char   name[TASK_NAME_LEN_MAX];
};

void sched_start(void);

/* non-posix syscalls */
int procstat(struct procstat_info info[TASK_CNT_MAX]);
void setprogname(const char *name);
const char *getprogname(void);
int delay_ticks(uint32_t ticks);
pid_t gettid(void);

#endif
