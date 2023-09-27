#ifndef __KERNEL_INTERRUPT_H__
#define __KERNEL_INTERRUPT_H__

typedef void (*irq_handler_t)(void);

void preempt_disable(void);
void preempt_enable(void);

void irq_init(void);
int request_irq(unsigned int irq, irq_handler_t handler,
                unsigned long flags, const char *name,
                void *dev);

#endif
