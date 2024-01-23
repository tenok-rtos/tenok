/**
 * @file
 */
#ifndef __KERNEL_PREEMPT_H__
#define __KERNEL_PREEMPT_H__

void __preempt_disable(void);
void __preempt_enable(void);

void preempt_count_inc(void);
void preempt_count_dec(void);
int preempt_count(void);
void preempt_count_set(uint32_t count);

/**
 * @brief  Disable all interrupts
 * @param  None
 * @retval None
 */
void preempt_disable(void);

/**
 * @brief  Enable all interrupts
 * @param  None
 * @retval None
 */
void preempt_enable(void);

#endif
