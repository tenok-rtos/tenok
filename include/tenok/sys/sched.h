/**
 * @file
 */
#ifndef __SYS_SCHED_H__
#define __SYS_SCHED_H__

#define SCHED_FIFO 1
#define SCHED_RR 2
#define SCHED_OTHER 3
#define SCHED_SPORADIC 4

struct sched_param {
    int sched_priority;
};

#endif
