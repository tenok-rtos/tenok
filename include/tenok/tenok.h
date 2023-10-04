#ifndef __TENOK_H__
#define __TENOK_H__

#include <stdint.h>
#include <stdbool.h>
#include <sys/types.h>

#include <kernel/kernel.h>

#include "kconfig.h"

struct procstat_info {
    int      pid;
    int      priority;
    int      status;
    uint32_t privilege;
    char     name[TASK_NAME_LEN_MAX];
};

/* non-posix syscalls */
int procstat(struct procstat_info info[TASK_CNT_MAX]);
void setprogname(const char *name);
const char *getprogname(void);
uint32_t delay_ticks(uint32_t ticks);
pid_t gettid(void);

#endif
