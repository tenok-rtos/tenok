/**
 * @file
 */
#ifndef __KERNEL_INTERRUPT_H__
#define __KERNEL_INTERRUPT_H__

/**
 * @brief  Disable all interrupts
 * @param  None
 * @retval None
 */
void preempt_disable(void);

/**
 * @brief  Ensable all interrupts. The function is only valid if it is called by
 *         kernel space code or in kernel threads
 * @param  None
 * @retval None
 */
void preempt_enable(void);

/**
 * @brief  Initialize the interrupt vector table of the processor
 * @param  None
 * @retval None
 */
void irq_init(void);

#endif
