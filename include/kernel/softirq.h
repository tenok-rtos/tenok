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
 * @brief  Initialize the tasklet
 * @param  t: Pointer to the tasklet.
 * @param  func: The tasklet function to execute if scheduled.
 * @param  data: The data passed with the tasklet function.
 * @retval None
 */
void tasklet_init(struct tasklet_struct *t,
                  void (*func)(unsigned long),
                  unsigned long data);

/**
 * @brief  Schedule the tasklet
 * @param  t: Pointer to the tasklet.
 * @retval None
 */
void tasklet_schedule(struct tasklet_struct *t);

void softirqd(void);

#endif
