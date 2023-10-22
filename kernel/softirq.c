#include <tenok.h>
#include <unistd.h>
#include <sched.h>

#include <kernel/kernel.h>
#include <kernel/softirq.h>
#include <kernel/list.h>
#include <kernel/wait.h>
#include <kernel/interrupt.h>

#include "kconfig.h"

#define SOFTIRQD_PID 2

extern struct thread_info threads[TASK_CNT_MAX];
extern struct list_head ready_list[TASK_MAX_PRIORITY + 1];

struct list_head tasklet_list;

void tasklet_init(struct tasklet_struct *t,
                  void (*func)(unsigned long), unsigned long data)
{
    t->func = func;
    t->data = data;
}

void tasklet_schedule(struct tasklet_struct *t)
{
    /* schedule the tasklet */
    list_move(&t->list, &tasklet_list);

    /* wake up the softirq daemon */
    struct thread_info *thread = acquire_thread(SOFTIRQD_PID);
    finish_wait(&thread->list);
}

void softirqd(void)
{
    setprogname("softirqd");

    struct tasklet_struct *t;

    while(1) {
        if(list_empty(&tasklet_list)) {
            pause();
        } else {
            preempt_disable();

            /* retrieve tasklet */
            t = list_first_entry(&tasklet_list, struct tasklet_struct, list);
            list_del(&t->list);

            /* execute the tasklet */
            t->func(t->data);

            preempt_enable();
        }
    }
}

void softirq_init(void)
{
    INIT_LIST_HEAD(&tasklet_list);

    /* create softirq daemon for handling tasklets  */
    kthread_create(softirqd, KERNEL_INT_PRI, 1024);
}
