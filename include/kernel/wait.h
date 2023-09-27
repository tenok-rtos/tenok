#ifndef __KERNEL_WAIT_H__
#define __KERNEL_WAIT_H__

#include <stdbool.h>

#include <kernel/list.h>

#define init_waitqueue_head(wq) list_init(wq)

typedef struct list wait_queue_head_t;

void prepare_to_wait(struct list *q, struct list *wait, int state);
void wake_up(struct list *wait_list);
void wait_event(struct list *wq, bool condition);

#endif
