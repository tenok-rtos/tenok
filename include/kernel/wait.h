/**
 * @file
 */
#ifndef __KERNEL_WAIT_H__
#define __KERNEL_WAIT_H__

#include <stdbool.h>

#include <common/list.h>
#include <kernel/kernel.h>
#include <kernel/sched.h>
#include <kernel/thread.h>

typedef struct list_head wait_queue_head_t;

/**
 * @brief  Declare and initialize a wait queue
 * @param  name: Name of the wait queue variable.
 * @retval None
 */
#define DECLARE_WAIT_QUEUE_HEAD(name) LIST_HEAD(name)

/**
 * @brief  Initialize the wait queue
 * @param  list: The wait queue to initialize.
 * @retval None
 */
#define init_waitqueue_head(wq) INIT_LIST_HEAD(wq)

/**
 * @brief  Wait until the condition became true
 * @param  wq_head: The wait queue to sleep.
 * @param  condition: The condition to wait until true.
 * @retval None
 */
#define wait_event(wq_head, condition)                           \
    do {                                                         \
        CURRENT_THREAD_INFO(curr_thread);                        \
        while (1) {                                              \
            if (condition)                                       \
                break;                                           \
            prepare_to_wait(&wq_head, curr_thread, THREAD_WAIT); \
            schedule();                                          \
        }                                                        \
    } while (0)

/**
 * @brief  Suspend current thread and place it into a wait list with a new
 *         state
 * @param  wait_list: Thread waiting list.
 * @param  thread: The thread to to place in the wait list.
 * @param  state: The new state of the thread.
 * @retval None
 */
void prepare_to_wait(struct list_head *wait_list,
                     struct thread_info *thread,
                     int state);

/**
 * @brief  Wake up the highest priority thread from the wait list
 * @param  wait_list: The wait list that contains suspended threads.
 * @retval None
 */
void wake_up(struct list_head *wait_list);

/**
 * @brief  Wake up all threads from the wait list
 * @param  wait_list: The wait list that contains suspended threads.
 * @retval None
 */
void wake_up_all(struct list_head *wait_list);

/**
 * @brief  Move the thread from a wait list into a ready list and
 *         set it to be ready
 * @param  wait: The pointer that point to the thread to wake up.
 * @retval None
 */
void finish_wait(struct thread_info *thread);

#endif
