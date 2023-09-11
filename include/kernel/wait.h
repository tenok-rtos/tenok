#ifndef __KERNEL_WAIT_H__
#define __KERNEL_WAIT_H__

#include <stdbool.h>

#include <kernel/list.h>

typedef struct list wait_queue_head_t;

void wake_up(struct list *wait_list);
bool wait_event(struct list *wq, bool condition);

#endif
