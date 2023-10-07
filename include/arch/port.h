/**
 * @file
 */
#ifndef __PORT_H__
#define __PORT_H__

#include <stdint.h>
#include <stdbool.h>

#define NACKED __attribute__ ((naked))

#define SYSCALL(num) \
    asm volatile ("push {r7}   \n" \
                  "mov  r7, %0 \n" \
                  "svc  0      \n" \
                  "pop  {r7}   \n" \
                  "bx lr       \n" \
                  :: "i"(num))

/**
 * @brief  Get the current ARM processor mode
 * @retval uint32_t: The ISR_NUMBER field of the IPSR register.
 */
uint32_t get_proc_mode(void);

/**
 * @brief  Initialze the operating system to split kernel space and user space
 * @param  A dummy stack memory space to provide.
 */
void os_env_init(uint32_t stack);

/**
 * @brief  Jump to the thread given by the stack (context switch)
 * @param  stack: The thread stack to provide.
 * @param  stack: Specify the thread privilege. True as kernel thread and false
           as user thread.
 * @retval uint32_t*: New stack pointer after returning back to the kernel.
 */
uint32_t *jump_to_thread(uint32_t stack, bool privileged);

void __platform_init(void);

#endif
