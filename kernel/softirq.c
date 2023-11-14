#include <sched.h>
#include <tenok.h>
#include <unistd.h>

#include <kernel/interrupt.h>
#include <kernel/kernel.h>
#include <kernel/list.h>
#include <kernel/softirq.h>
#include <kernel/wait.h>

#include "kconfig.h"

#define SOFTIRQD_ID 1

static LIST_HEAD(tasklet_list);

void tasklet_init(struct tasklet_struct *t,
                  void (*func)(unsigned long),
                  unsigned long data)
{
    t->func = func;
    t->data = data;
}

void tasklet_schedule(struct tasklet_struct *t)
{
    /* schedule the tasklet */
    list_move(&t->list, &tasklet_list);

    /* wake up the softirq daemon */
    struct thread_info *thread = acquire_thread(SOFTIRQD_ID);
    finish_wait(&thread->list);
}

void softirqd(void)
{
    setprogname("softirqd");

    struct tasklet_struct *t;

    while (1) {
        if (list_empty(&tasklet_list)) {
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
