#include <kernel/kernel.h>
#include <kernel/softirq.h>
#include <kernel/list.h>
#include <kernel/wait.h>
#include <kernel/interrupt.h>

#include <tenok/tenok.h>
#include <tenok/unistd.h>
#include <tenok/sched.h>

#include "kconfig.h"

#define SOFTIRQD_PID 1

extern struct thread_info threads[TASK_CNT_MAX];
extern struct list ready_list[TASK_MAX_PRIORITY + 1];

struct list tasklet_list;

void tasklet_init(struct tasklet_struct *t,
                  void (*func)(unsigned long), unsigned long data)
{
    t->func = func;
    t->data = data;
}

void tasklet_schedule(struct tasklet_struct *t)
{
    list_move(&t->list, &tasklet_list);

    /* wake up the softirq daemon if it is sleeping */
    struct thread_info *task = &threads[SOFTIRQD_PID];
    if(task->status == TASK_WAIT) {
        list_move(&task->list, &ready_list[task->priority]);
        task->status = TASK_READY;
    }
}

void softirqd(void)
{
    setprogname("softirqd");

    while(1) {
        if(list_is_empty(&tasklet_list)) {
            pause();
        } else {
            /* pick up the tasklet */
            preempt_disable();
            struct list *l = list_pop(&tasklet_list);
            preempt_enable();

            /* execute the tasklet */
            struct tasklet_struct *t = list_entry(l, struct tasklet_struct, list);
            t->func(t->data);
        }
    }
}

void softirq_init(void)
{
    list_init(&tasklet_list);

    /* create softirq daemon for handling tasklets  */
    ktask_create(softirqd, KERNEL_INT_PRI, TASK_STACK_SIZE);
}
