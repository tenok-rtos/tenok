#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <arch/port.h>
#include <kernel/interrupt.h>
#include <kernel/kernel.h>
#include <kernel/tty.h>

#include "stm32f4xx.h"

#define IRQ_START 16
#define IRQ_CNT 98

#define ISR __attribute__((weak, alias("Default_Handler")))

#define FAULT_DUMP(type)                  \
    do {                                  \
        asm volatile(                     \
            "mov r0, %0   \n"             \
            "mrs r1, msp  \n"             \
            "mrs r2, psp  \n"             \
            "mov r3, lr   \n"             \
            "bl fault_dump\n" ::"r"(type) \
            :);                           \
    } while (0)

enum {
    HARD_FAULT = 0,
    MPU_FAULT = 1,
    BUS_FAULT = 2,
    USAGE_FAULT = 3,
};

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

static void early_printf(char *format, ...)
{
    va_list args;
    va_start(args, format);

    char buf[200] = {'\r', 0};
    vsnprintf(buf, 200, format, args);
    early_tty_print(buf, strlen(buf));

    va_end(args);
}

static void dump_registers(uint32_t *fault_stack)
{
    unsigned int r0 = fault_stack[0];
    unsigned int r1 = fault_stack[1];
    unsigned int r2 = fault_stack[2];
    unsigned int r3 = fault_stack[3];
    unsigned int r12 = fault_stack[4];
    unsigned int lr = fault_stack[5];
    unsigned int pc = fault_stack[6];
    unsigned int psr = fault_stack[7];

    early_printf(
        "r0: 0x%08x r1:  0x%08x r2: 0x%08x\n\r"
        "r3: 0x%08x r12: 0x%08x lr: 0x%08x\n\r"
        "pc: 0x%08x psr: 0x%08x\n\r",
        r0, r1, r2, r3, r12, lr, pc, psr);
}

void fault_dump(uint32_t fault_type, uint32_t *msp, uint32_t *psp, uint32_t lr)
{
    CURRENT_THREAD_INFO(curr_thread);

    bool imprecise_error = false;
    uint32_t *fault_stack = NULL;

    if (lr == 0xfffffff1 || lr == 0xffffffe1) {
        imprecise_error = true;
    } else if (lr == 0xfffffff9 || lr == 0xffffffe9) {
        fault_stack = msp;
    } else if (lr == 0xfffffffd || lr == 0xffffffed) {
        fault_stack = psp;
    }

    switch (fault_type) {
    case HARD_FAULT:
        early_printf("\r================ HARD FAULT ==================\n\r");
        break;
    case MPU_FAULT:
        early_printf("\r================= MPU FAULT ==================\n\r");
        break;
    case BUS_FAULT:
        early_printf("\r================= BUS FAULT ==================\n\r");
        break;
    case USAGE_FAULT:
        early_printf("\r================ USAGE FAULT =================\n\r");
        break;
    }

    if (curr_thread) {
        early_printf("Current thread: %p (%s)\n\r", curr_thread,
                     curr_thread->name);
    }

    if (!imprecise_error) {
        dump_registers(fault_stack);
        early_printf("Faulting instruction address = 0x%08x\n\r",
                     fault_stack[6]);
    } else {
        early_printf(
            "Imprecise fault detected\n\r"
            "Unable to dump registers\n\r");
    }

    early_printf(
        ">>> Halting system <<<\n\r"
        "==============================================");

    while (1)
        ;
}

void NMI_Handler(void)
{
    while (1)
        ;
}

NACKED void HardFault_Handler(void)
{
    FAULT_DUMP(HARD_FAULT);
}

void MemManage_Handler(void)
{
    FAULT_DUMP(MPU_FAULT);
}

void BusFault_Handler(void)
{
    FAULT_DUMP(BUS_FAULT);
}

void UsageFault_Handler(void)
{
    FAULT_DUMP(USAGE_FAULT);
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
