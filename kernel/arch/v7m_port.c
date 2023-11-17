#include <stdint.h>
#include <string.h>

#include <arch/port.h>
#include <kernel/kernel.h>
#include <kernel/printk.h>

#include "kconfig.h"
#include "stm32f4xx.h"

#define THREAD_PSP 0xFFFFFFFD
#define INITIAL_XPSR 0x01000000

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

struct context {
    /* pushed by the os: */
    uint32_t r4_to_r11[8]; /* r4, ..., r11 */
    uint32_t _lr;
    uint32_t _r7; /* r7 (syscall number) */

    /* pushed by the exception entry: */
    uint32_t r0, r1, r2, r3;
    uint32_t r12_lr_pc_xpsr[4]; /* r12, lr, pc, xpsr */
};

struct context_fpu {
    /* pushed by the os: */
    uint32_t r4_to_r11[8]; /* r4, ..., r11 */
    uint32_t _lr;
    uint32_t _r7; /* r7 (syscall number) */

    /* pushed by the exception entry: */
    uint32_t s16_to_s31[16]; /* s16, ..., s31 */
    uint32_t r0, r1, r2, r3;
    uint32_t r12_lr_pc_xpsr[4];   /* r12, lr, pc, xpsr */
    uint32_t s0_to_s15_fpscr[17]; /* s0, ..., s15, fpscr */
};

uint32_t get_proc_mode(void)
{
    /* get the 9 bits isr number from the ipsr register,
     * check "exception types" of the ARM CM3 for details
     */
    volatile unsigned int mode = 0;
    asm volatile(
        "mrs  r0, ipsr \n"
        "str  r0, [%0] \n" ::"r"(&mode));
    return mode & 0x1ff;  // return ipsr[8:0]
}

void preempt_disable(void)
{
    asm volatile(
        "cpsid i \n"
        "cpsid f \n");
}

void preempt_enable(void)
{
    asm volatile(
        "cpsie i \n"
        "cpsie f \n");
}

void jump_to_kernel(void)
{
    SCB->ICSR |= SCB_ICSR_PENDSVSET_Msk;
}

void __stack_init(uint32_t **stack_top,
                  uint32_t func,
                  uint32_t return_handler,
                  uint32_t args[4])
{
    /* the stack design contains three parts:
     * xpsr, pc, lr, r12, r3, r2, r1, r0, (for setup exception return),
     * _r7 (for passing system call number), and
     * _lr, r11, r10, r9, r8, r7, r6, r5, r4 (for context switch)
     */
    uint32_t *sp = *stack_top - 18;
    *stack_top = sp;
    memset(sp, 0, sizeof(uint32_t) * 18);
    sp[17] = INITIAL_XPSR;    // psr
    sp[16] = func;            // pc
    sp[15] = return_handler;  // lr
    sp[13] = args[3];         // r3
    sp[12] = args[2];         // r2
    sp[11] = args[1];         // r1
    sp[10] = args[0];         // r0
    sp[8] = THREAD_PSP;       //_lr
}

bool check_systick_event(void *sp)
{
    uint32_t lr = ((uint32_t *) sp)[8];
    long *syscall_num = NULL;

    /* EXC_RETURN[4]: 0 = FPU used / 1 = FPU unused */
    if (lr & 0x10) {
        syscall_num = (long *) &((struct context *) sp)->_r7;
    } else {
        syscall_num = (long *) &((struct context_fpu *) sp)->_r7;
    }

    /* if r7 is negative, the kernel is returned from the systick
     * interrupt. otherwise the kernel is returned from the
     * supervisor call.
     */
    if (*syscall_num < 0) {
        *syscall_num *= -1; /* restore the syscall number */
        return true;
    }

    return false;
}

unsigned long get_syscall_info(void *sp, unsigned long *pargs[4])
{
    uint32_t lr = ((uint32_t *) sp)[8];
    unsigned long syscall_num;

    /* EXC_RETURN[4]: 0 = FPU used / 1 = FPU unused */
    if (lr & 0x10) {
        struct context *context = (struct context *) sp;
        syscall_num = context->_r7;
        pargs[0] = &context->r0;
        pargs[1] = &context->r1;
        pargs[2] = &context->r2;
        pargs[3] = &context->r3;
    } else {
        struct context_fpu *context = (struct context_fpu *) sp;
        syscall_num = context->_r7;
        pargs[0] = &context->r0;
        pargs[1] = &context->r1;
        pargs[2] = &context->r2;
        pargs[3] = &context->r3;
    }

    return syscall_num;
}

void __idle(void)
{
    asm volatile("wfi");
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

NACKED void HardFault_Handler(void)
{
    FAULT_DUMP(HARD_FAULT);
}

NACKED void MemManage_Handler(void)
{
    FAULT_DUMP(MPU_FAULT);
}

NACKED void BusFault_Handler(void)
{
    FAULT_DUMP(BUS_FAULT);
}

NACKED void UsageFault_Handler(void)
{
    FAULT_DUMP(USAGE_FAULT);
}
