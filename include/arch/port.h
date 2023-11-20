/**
 * @file
 */
#ifndef __PORT_H__
#define __PORT_H__

#include <stdbool.h>
#include <stdint.h>

#define NACKED __attribute__((naked))

#define SYSCALL(num)     \
    asm volatile(        \
        "push {r7}   \n" \
        "mov  r7, %0 \n" \
        "svc  0      \n" \
        "pop  {r7}   \n" \
        "bx lr       \n" ::"i"(num))

/**
 * @brief  Get the current ARM processor mode
 * @retval uint32_t: The ISR_NUMBER field of the IPSR register.
 */
uint32_t get_proc_mode(void);

/**
 * @brief  Initialze the operating system to split kernel space and user space
 * @param  A dummy stack memory space to provide.
 */
void os_env_init(void *stack);

/**
 * @brief  Jump to the thread given by the stack (context switch)
 * @param  stack: The thread stack to provide.
 * @param  stack: Specify the thread privilege. True as kernel thread and false
 *         as user thread.
 * @retval uint32_t*: New stack pointer after returning back to the kernel.
 */
void *jump_to_thread(void *stack, bool privileged);

/**
 * @brief  Jump to the kernel space
 * @param  None
 * @retval None
 */
void jump_to_kernel(void);

/**
 * @brief  Basic platform initialization
 * @param  None
 * @retval None
 */
void __platform_init(void);

/**
 * @brief  Initialize board drivers
 * @param  None
 * @retval None
 */
void __board_init(void);

/**
 * @brief  Initialize thread stack
 * @param  stack_top: The pointer of the stack pointer.
 * @param  func: The function to execute after context switch.
 * @param  return_handler: The return handler function after fuc returned.
 * @param  args[4]: The arguments for func.
 * @retval None
 */
void __stack_init(uint32_t **stack_top,
                  uint32_t func,
                  uint32_t return_handler,
                  uint32_t args[4]);

/**
 * @brief  Check if the kernel is returned from user space via systick
 * @param  sp: The stack pointer points to the top of the thread stack.
 * @retval bool: true if is returned via systick; otherwise false.
 */
bool check_systick_event(void *sp);

/**
 * @brief  Get syscall number and arguments
 * @param  sp: The stack pointer points to the top of the thread stack.
 * @param  pargs: The array for returning syscall arguments.
 * @retval unsigned long: The current syscall number.
 */
unsigned long get_syscall_info(void *sp, unsigned long *pargs[4]);

/**
 * @brief  Trigger platform-specific idling
 * @param  None
 * @retval None
 */
void __idle(void);

#endif
