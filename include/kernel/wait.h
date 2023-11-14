/**
 * @file
 */
#ifndef __KERNEL_WAIT_H__
#define __KERNEL_WAIT_H__

#include <stdbool.h>

#include <common/list.h>

#define init_waitqueue_head(wq) INIT_LIST_HEAD(wq)

#define init_wait(wait)                       \
    do {                                      \
        CURRENT_THREAD_INFO(__curr_thread__); \
        wait = &__curr_thread__->list;        \
    } while (0)

typedef struct list_head wait_queue_head_t;
typedef struct list_head wait_queue_t;

/**
 * @brief  Suspend the current thread and place it into a wait list with a new
           state
 * @param  q: The thread list
 * @param  wait: The thread wait list
 * @param  state: The new state of the thread
 * @retval None
 */
void prepare_to_wait(struct list_head *q, struct list_head *wait, int state);

/**
 * @brief  Wake up the highest priority thread from the wait_list
 * @param  wait_list: The wait list that contains suspended threads
 * @retval None
 */
void wake_up(struct list_head *wait_list);

/**
 * @brief  Wake up all threads from the wait_list
 * @param  wait_list: The wait list that contains suspended threads
 * @retval None
 */
void wake_up_all(struct list_head *wait_list);

/**
 * @brief  Move the thread from a waiting list into the ready list and
           set it to ready state
 * @param  thread_list: The list head of the thread.
 * @retval None
 */
void finish_wait(struct list_head *thread_list);

#endif
