#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <arch/port.h>
#include <kernel/kernel.h>
#include <kernel/tty.h>

#include "stm32f4xx.h"

#define IRQ_START 16
#define IRQ_CNT 98

#define ISR __attribute__((weak, alias("Default_Handler")))

void Default_Handler(void);
void irq_handler(void);

void Reset_Handler(void);
void NMI_Handler(void);
void HardFault_Handler(void);
void MemManage_Handler(void);
void BusFault_Handler(void);
void UsageFault_Handler(void);
void SVC_Handler(void);
void DebugMon_Handler(void);
void PendSV_Handler(void);
void SysTick_Handler(void);

extern uint32_t _estack;

typedef void (*irq_handler_t)(void);

__attribute__((section(".isr_vector"))) irq_handler_t isr_vectors[IRQ_CNT] = {
    (irq_handler_t) &_estack,
    /* exceptions */
    Reset_Handler,
    NMI_Handler,
    HardFault_Handler,
    MemManage_Handler,
    BusFault_Handler,
    UsageFault_Handler,
    0,
    0,
    0,
    0,
    SVC_Handler,
    DebugMon_Handler,
    0,
    PendSV_Handler,
    SysTick_Handler,
    /* interrupts */
    irq_handler,
    irq_handler,
    irq_handler,
    irq_handler,
    irq_handler,
    irq_handler,
    irq_handler,
    irq_handler,
    irq_handler,
    irq_handler,
    irq_handler,
    irq_handler,
    irq_handler,
    irq_handler,
    irq_handler,
    irq_handler,
    irq_handler,
    irq_handler,
    irq_handler,
    irq_handler,
    irq_handler,
    irq_handler,
    irq_handler,
    irq_handler,
    irq_handler,
    irq_handler,
    irq_handler,
    irq_handler,
    irq_handler,
    irq_handler,
    irq_handler,
    irq_handler,
    irq_handler,
    irq_handler,
    irq_handler,
    irq_handler,
    irq_handler,
    irq_handler,
    irq_handler,
    irq_handler,
    irq_handler,
    irq_handler,
    irq_handler,
    irq_handler,
    irq_handler,
    irq_handler,
    irq_handler,
    irq_handler,
    irq_handler,
    irq_handler,
    irq_handler,
    irq_handler,
    irq_handler,
    irq_handler,
    irq_handler,
    irq_handler,
    irq_handler,
    irq_handler,
    irq_handler,
    irq_handler,
    irq_handler,
    irq_handler,
    irq_handler,
    irq_handler,
    irq_handler,
    irq_handler,
    irq_handler,
    irq_handler,
    irq_handler,
    irq_handler,
    irq_handler,
    irq_handler,
    irq_handler,
    irq_handler,
    irq_handler,
    irq_handler,
    irq_handler,
    irq_handler,
    irq_handler,
    irq_handler,
    irq_handler,
    irq_handler,
};

irq_handler_t irq_table[IRQ_CNT];

void Default_Handler(void)
{
    while (1)
        ;
}

void SysTick_Handler(void)
{
    jump_to_kernel();
}

void NMI_Handler(void)
{
    while (1)
        ;
}

void HardFault_Handler(void)
{
    CURRENT_THREAD_INFO(curr_thread);

    char s[150] = {0};
    snprintf(s, PRINT_SIZE_MAX,
             "***** HARD FAULT *****\n\r"
             "Current thread: %s (PID: %d, TID: %d)\n\r"
             "Fatal fault encountered. Aborting!",
             curr_thread->name, curr_thread->task->pid, (int) curr_thread->tid);

    early_tty_print(s, strlen(s));

    while (1)
        ;
}

void MemManage_Handler(void)
{
    while (1)
        ;
}

void BusFault_Handler(void)
{
    while (1)
        ;
}

void UsageFault_Handler(void)
{
    while (1)
        ;
}

void DebugMon_Handler(void)
{
}

void irq_handler(void)
{
    int irq = get_proc_mode();
    irq_table[irq]();
}

void irq_init(void)
{
    for (int i = IRQ_START; i < IRQ_CNT; i++) {
        irq_table[i] = Default_Handler;
    }

    /* priority range of group 4 is 0-15 */
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_4);
    NVIC_SetPriority(MemoryManagement_IRQn, 0);
    NVIC_SetPriority(BusFault_IRQn, 0);
    NVIC_SetPriority(UsageFault_IRQn, 0);
    NVIC_SetPriority(SysTick_IRQn, 1);
    NVIC_SetPriority(SVCall_IRQn, 15);
    NVIC_SetPriority(PendSV_IRQn, 15);

    /* enable the systick timer */
    SysTick_Config(SystemCoreClock / OS_TICK_FREQ);
}

int request_irq(unsigned int irq,
                irq_handler_t handler,
                unsigned long flags,
                const char *name,
                void *dev)
{
    irq_table[IRQ_START + irq] = handler;
    return 0;
}
