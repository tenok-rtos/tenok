#ifndef __SCHED_H__
#define __SCHED_H__

int sched_get_priority_max(int policy);
int sched_get_priority_min(int policy);
int sched_yield(void);

#endif
