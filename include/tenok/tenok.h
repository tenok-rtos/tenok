/**
 * @file
 */
#ifndef __TENOK_H__
#define __TENOK_H__

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

#include "kconfig.h"

struct thread_stat {
    int pid;
    int tid;
    int priority;
    char *status;
    bool privileged;
    size_t stack_usage;
    size_t stack_size;
    char name[THREAD_NAME_MAX];
};

enum {
    PAGE_TOTAL_SIZE = 0,
    PAGE_FREE_SIZE = 1,
    HEAP_TOTAL_SIZE = 2,
    HEAP_FREE_SIZE = 3
} MINFO_NAMES;

/**
 * @brief  Start the operating system
 * @param  None
 * @retval None
 */
void sched_start(void);

/**
 * @brief  Get the thread information iteratively
 * @param  info: For returning thread information.
 * @param  next: The pointer to the the next thread. The initial argument
 *         should be set with NULL.
 * @retval void *: The pointer to the next thread. The function returns
 *         NULL if next thread does not exist.
 */
void *thread_info(struct thread_stat *info, void *next);

/**
 * @brief  Set the name of the running thread
 * @param  name: The name of the program.
 * @retval None
 */
void setprogname(const char *name);

/**
 * @brief  To cause the calling thread to sleep for the given ticks
 * @param  ticks: The ticks of time to sleep
 * @retval int: 0 on success and nonzero error number on error.
 */
int delay_ticks(uint32_t ticks);

/**
 * @brief  Get memory information of the system
 * @param  name: The information to acquire (check MINFO_NAMES).
 * @retval int: For returning the acquired information.
 */
int minfo(int name);

#endif
