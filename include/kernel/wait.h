/**
 * @file
 */
#ifndef __KERNEL_WAIT_H__
#define __KERNEL_WAIT_H__

#include <stdbool.h>

#include <kernel/list.h>

#define init_waitqueue_head(wq) list_init(wq)

typedef struct list wait_queue_head_t;

/**
 * @brief  Suspend the current thread and place it into a wait list with a new
           state
 * @param  q: The thread list
 * @param  wait: The thread wait list
 * @param  state: The new state of the thread
 * @retval None
 */
void prepare_to_wait(struct list *q, struct list *wait, int state);

/**
 * @brief  Wake up the highest priority task from the wait_list
 * @param  wait_list: The wait list that contains suspended tasks
 * @retval None
 */
void wake_up(struct list *wait_list);

/**
 * @brief  Suspend the current thread and put it into the wait queue wq if the
           condition is set false
 * @param  wq: The wait queue to provide.
 * @param  condition: The task will suspend if the variable is set false.
 * @retval None
 */
void wait_event(struct list *wq, bool condition);

#endif
