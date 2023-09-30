/**
 * @file
 */
#ifndef __KERNEL_INTERRUPT_H__
#define __KERNEL_INTERRUPT_H__

typedef void (*irq_handler_t)(void);

/**
 * @brief  Disable interrupts with priority lower than KERNEL_INT_PRI. The function
           is only valid if it is called by kernel space code or in kernel threads
 * @param  None
 * @retval None
 */
void preempt_disable(void);

/**
 * @brief  Eisable all interrupts. The function is only valid if it is called by kernel
           space code or in kernel threads
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

/**
 * @brief  Register the interrupt handler function to a specified IRQ
 * @param  irq: The IRQ number to provide.
 * @param  handler: The interrupt handler function to provide.
 * @param  flags: Not used.
 * @param  name: Not used.
 * @param  dev: Not used.
 * @retval int: 0 on sucess and nonzero error number on error.
 */
int request_irq(unsigned int irq, irq_handler_t handler,
                unsigned long flags, const char *name,
                void *dev);

#endif
