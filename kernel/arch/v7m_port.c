#include <stdint.h>
#include <string.h>

#include "kconfig.h"

#define HANDLER_MSP  0xFFFFFFF1
#define THREAD_MSP   0xFFFFFFF9
#define THREAD_PSP   0xFFFFFFFD

#define INITIAL_XPSR 0x01000000

uint32_t get_proc_mode(void)
{
    /*
     * get the 9 bits isr number from the ipsr register,
     * check "exception types" of the ARM CM3 for details
     */
    volatile unsigned int mode = 0;
    asm volatile ("mrs  r0, ipsr \n"
                  "str  r0, [%0] \n"
                  :: "r"(&mode));
    return mode & 0x1ff; //return ipsr[8:0]
}

void preempt_disable(void)
{
    /* set base pri */
    asm volatile ("mov r0,      %0\n"
                  "msr basepri, r0\n"
                  :: "i"(KERNEL_INT_PRI << 4));
}

void preempt_enable(void)
{
    /* reset base pri */
    asm volatile ("mov r0,      #0\n"
                  "msr basepri, r0\n");
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
    memset(sp, 0, 18);
    sp[17] = INITIAL_XPSR;   //psr
    sp[16] = func;           //pc
    sp[15] = return_handler; //lr
    sp[13] = args[3];        //r3
    sp[12] = args[2];        //r2
    sp[11] = args[1];        //r1
    sp[10] = args[0];        //r0
    sp[8]  = THREAD_PSP;     //_lr
}
