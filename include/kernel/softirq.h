/**
 * @file
 */
#ifndef __KERNEL_SOFTIRQ_H__
#define __KERNEL_SOFTIRQ_H__

struct tasklet_struct {
    void (*func)(unsigned long);
    unsigned long data;
    struct list_head list;
};

/**
 * @brief  Initialize the given tasklet
 * @param  t: Tasklet to provide.
 * @param  func: The tasklet function to execute if scheduled.
 * @param  data: The data pass to the tasklet function if scheduled.
 * @retval None
 */
void tasklet_init(struct tasklet_struct *t,
                  void (*func)(unsigned long), unsigned long data);

/**
 * @brief  Schedule the given tasklet to execute by softirqd
 * @param  t: Tasklet to provide.
 * @retval None
 */
void tasklet_schedule(struct tasklet_struct *t);

void softirqd(void);

#endif
