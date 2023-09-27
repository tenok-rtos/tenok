#ifndef __PORT_H__
#define __PORT_H__

#include <stdint.h>

#define NACKED __attribute__ ((naked))

#define SYSCALL(num) \
    asm volatile ("push {r7}   \n" \
                  "mov  r7, %0 \n" \
                  "svc  0      \n" \
                  "nop         \n" \
                  "pop  {r7}   \n" \
                  "bx lr       \n" \
                  :: "i"(num))

uint32_t get_proc_mode(void);

#endif
