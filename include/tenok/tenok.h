#ifndef __TENOK_H__
#define __TENOK_H__

#include "kconfig.h"

struct procstat_info {
    int pid;
    int priority;
    int status;
    char name[TASK_NAME_LEN_MAX];
};

/* non-posix syscalls */
int procstat(struct procstat_info info[TASK_CNT_MAX]);
void setprogname(const char *name);
const char *getprogname(void);
uint32_t delay_ticks(uint32_t ticks);

#endif
