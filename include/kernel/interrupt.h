/**
 * @file
 */
#ifndef __KERNEL_INTERRUPT_H__
#define __KERNEL_INTERRUPT_H__

void __preempt_disable(void);
void __preempt_enable(void);

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
