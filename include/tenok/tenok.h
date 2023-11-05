/**
 * @file
 */
#ifndef __TENOK_H__
#define __TENOK_H__

#include <stdint.h>
#include <stdbool.h>
#include <sys/types.h>

#include <kernel/kernel.h>

#include "kconfig.h"

struct thread_stat {
    int    pid;
    int    tid;
    int    priority;
    int    status;
    bool   privileged;
    size_t stack_usage;
    size_t stack_size;
    char   name[THREAD_NAME_MAX];
};

enum {
    PAGE_TOTAL_SIZE = 0,
    PAGE_FREE_SIZE = 1,
    HEAP_TOTAL_SIZE = 2,
    HEAP_FREE_SIZE = 3
} MINFO_NAMES;

void sched_start(void);

/* non-posix syscalls */
void *thread_info(struct thread_stat *info, void *next);
void setprogname(const char *name);
const char *getprogname(void);
int delay_ticks(uint32_t ticks);
pid_t gettid(void);
int minfo(int name);

#endif
