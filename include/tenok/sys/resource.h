#ifndef __RESOURCE_H__
#define __RESOURCE_H__

#define PRIO_PROCESS 0
#define PRIO_PGRP    1
#define PRIO_USER    2

int getpriority(void);
int setpriority(int which, int who, int prio);

#endif
