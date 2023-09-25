#ifndef __KERNEL_SOFTIRQ_H__
#define __KERNEL_SOFTIRQ_H__

struct tasklet_struct {
    void (*func)(unsigned long);
    unsigned long data;
    struct list list;
};

void tasklet_init(struct tasklet_struct *t,
                  void (*func)(unsigned long), unsigned long data);
void tasklet_schedule(struct tasklet_struct *t);

void softirq_init(void);

#endif
