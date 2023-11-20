#include <sched.h>
#include <tenok.h>
#include <unistd.h>

#include <arch/port.h>
#include <common/list.h>
#include <kernel/interrupt.h>
#include <kernel/kernel.h>
#include <kernel/softirq.h>
#include <kernel/wait.h>

#include "kconfig.h"

static LIST_HEAD(tasklet_list);
static LIST_HEAD(softirqd_wait);

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
    int softirqd_tid = get_daemon_id(SOFTIRQD);
    struct thread_info *thread = acquire_thread(softirqd_tid);
    finish_wait(&thread->list);
}

static void softirqd_sleep(void)
{
    CURRENT_THREAD_INFO(curr_thread);

    preempt_disable();
    prepare_to_wait(&softirqd_wait, &curr_thread->list, THREAD_WAIT);
    jump_to_kernel();
    preempt_enable();
}

void softirqd(void)
{
    setprogname("softirqd");
    set_daemon_id(SOFTIRQD);

    struct tasklet_struct *t;

    while (1) {
        if (list_empty(&tasklet_list)) {
            softirqd_sleep();
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
