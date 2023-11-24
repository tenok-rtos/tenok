#include <stdint.h>
#include <stdio.h>
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
    /* Pushed by the OS */
    uint32_t r4_to_r11[8]; /* R4, ..., R11 */
    uint32_t _lr;
    uint32_t _r7; /* R7 (Syscall number) */

    /* Pushed by exception entry */
    uint32_t r0, r1, r2, r3;
    uint32_t r12_lr_pc_xpsr[4]; /* R12, LR, PC, PSR */
};

struct context_fpu {
    /* Pushed by the OS */
    uint32_t r4_to_r11[8]; /* R4, ..., R11 */
    uint32_t _lr;
    uint32_t _r7;            /* R7 (Syscall number) */
    uint32_t s16_to_s31[16]; /* S16, ..., S31 */

    /* Pushed by exception entry: */
    uint32_t r0, r1, r2, r3;
    uint32_t r12_lr_pc_xpsr[4];   /* R12, LR, PC, PSR */
    uint32_t s0_to_s15_fpscr[17]; /* S0, ..., S15, FPSCR */
};

uint32_t get_proc_mode(void)
{
    /* Get the 9 bits ISR number from the ipsr register.
     * Check "Exception types" of the ARM CM4 for details.
     */
    volatile unsigned int mode = 0;
    asm volatile(
        "mrs  r0, ipsr \n"
        "str  r0, [%0] \n" ::"r"(&mode));
    return mode & 0x1ff; /* return ipsr[8:0] */
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
    /* The stack design contains three parts:
     * XPSR, PC, LR, R12, R3, R2, R1, R0, (for setup exception return),
     * _R7 (for passing system call number), and
     * _LR, R11, R10, R9, R8, R7, R6, R5, R4 (for context switch).
     */
    uint32_t *sp = *stack_top - 18;
    *stack_top = sp;
    memset(sp, 0, sizeof(uint32_t) * 18);
    sp[17] = INITIAL_XPSR;       // PSR
    sp[16] = func & 0xfffffffe;  // PC
    sp[15] = return_handler;     // LR
    sp[13] = args[3];            // R3
    sp[12] = args[2];            // R2
    sp[11] = args[1];            // R1
    sp[10] = args[0];            // R0
    sp[8] = THREAD_PSP;          //_LR

    /* Note: The reason for clearing the LSB of the PC with 0xfffffffe is
     * because, by observation, the compiler may set the LSB of the function
     * pointer to indicate the Thumb state. However, this is an undefined
     * behavior on ARM Cortex-M processors, but most hardware ignores it
     * during execution. However, PC with 1 on the LSB can trigger a warning
     * in QEMU with the "-d guest_errors" option. This additional masking is
     * intended to clear such a warning in QEMU.
     * For detailed information, please refer to:
     * https://lists.gnu.org/archive/html/qemu-devel/2017-09/msg06469.html
     */
}

void __platform_init(void)
{
    /* Priority range of group 4 is 0-15 */
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_4);
    NVIC_SetPriority(MemoryManagement_IRQn, 0);
    NVIC_SetPriority(BusFault_IRQn, 0);
    NVIC_SetPriority(UsageFault_IRQn, 0);
    NVIC_SetPriority(SysTick_IRQn, 1);
    NVIC_SetPriority(SVCall_IRQn, 15);
    NVIC_SetPriority(PendSV_IRQn, 15);

    /* Enable SysTick timer */
    SysTick_Config(SystemCoreClock / OS_TICK_FREQ);

    /* Use a dummy stack to initialize the os environment */
    uint32_t stack_empty[32];
    os_env_init(&stack_empty[31]);
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

    /* If r7 is negative, then the kernel is returned from the SysTick
     * interrupt. Otherwise the kernel is returned via supervisor call.
     */
    if (*syscall_num < 0) {
        *syscall_num *= -1; /* Restore the syscall number */
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

static int dump_registers(char *buf, size_t buf_size, uint32_t *fault_stack)
{
    unsigned int r0 = fault_stack[0];
    unsigned int r1 = fault_stack[1];
    unsigned int r2 = fault_stack[2];
    unsigned int r3 = fault_stack[3];
    unsigned int r12 = fault_stack[4];
    unsigned int lr = fault_stack[5];
    unsigned int pc = fault_stack[6];
    unsigned int psr = fault_stack[7];

    return snprintf(buf, buf_size,
                    "r0: 0x%08x r1:  0x%08x r2: 0x%08x\n\r"
                    "r3: 0x%08x r12: 0x%08x lr: 0x%08x\n\r"
                    "pc: 0x%08x psr: 0x%08x\n\r",
                    r0, r1, r2, r3, r12, lr, pc, psr);
}

void fault_dump(uint32_t fault_type, uint32_t *msp, uint32_t *psp, uint32_t lr)
{
    CURRENT_THREAD_INFO(curr_thread);

    char *fault_location = "";
    uint32_t *fault_stack = NULL;

    if (lr == 0xfffffff1 || lr == 0xffffffe1) {
        fault_stack = msp;
        fault_location = "Fault location: IRQ Handler (sp = msp)\n";
    } else if (lr == 0xfffffff9 || lr == 0xffffffe9) {
        fault_stack = msp;
        fault_location = "Fault location: Kernel (sp = msp)\n";
    } else if (lr == 0xfffffffd || lr == 0xffffffed) {
        fault_stack = psp;
        fault_location = "Fault location: Thread (sp = psp)\n";
    }

    char *fault_type_s = "";
    switch (fault_type) {
    case HARD_FAULT:
        fault_type_s = "\r================ HARD FAULT ==================\n\r";
        break;
    case MPU_FAULT:
        fault_type_s = "\r================= MPU FAULT ==================\n\r";
        break;
    case BUS_FAULT:
        fault_type_s = "\r================= BUS FAULT ==================\n\r";
        break;
    case USAGE_FAULT:
        fault_type_s = "\r================ USAGE FAULT =================\n\r";
        break;
    }

    char thread_info_s[100] = {0};
    if (curr_thread) {
        snprintf(thread_info_s, sizeof(thread_info_s),
                 "Current thread: %p (%s)\n\r", curr_thread, curr_thread->name);
    }

    char reg_info_s[200] = {0};
    char fault_msg_s[100] = {0};
    dump_registers(reg_info_s, sizeof(reg_info_s), fault_stack);
    snprintf(fault_msg_s, sizeof(fault_msg_s),
             "Faulting instruction address = 0x%08lx\n\r", fault_stack[6]);

    panic(
        "%s%s%s%s%s"
        "Halting system\n\r"
        "==============================================",
        fault_type_s, thread_info_s, reg_info_s, fault_location, fault_msg_s);

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

void SysTick_Handler(void)
{
    jump_to_kernel();
}

void NMI_Handler(void)
{
    while (1)
        ;
}

void DebugMon_Handler(void)
{
}
