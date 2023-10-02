#include <stdint.h>

#include "kconfig.h"

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
