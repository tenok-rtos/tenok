#include <common/linkage.h>

.syntax unified

ENTRY(PendSV_Handler)
    /*Note: ISR always uses msp */

    /* Disable interrupts */
    cpsid i
    cpsid f
    
    neg   r7,  r7 /* Negate _r7 to indicate that the returning is triggered by SysTick */

    /* Save user state */
    mrs   r0,  psp  /* Load psp into the r0 */

    /* Save FPU state if required */
    tst      r14, #0x10
    it       eq
    vstmdbeq r0!, {s16-s31}

    stmdb r0!, {r7} /* Preserve syscall number */
    stmdb r0!, {r4, r5, r6, r7, r8, r9, r10, r11, lr} /* Preserve user state */

    /* Turn on the privilege mode */
    mov  r4, #0
    msr  control, r4

    /* Load kernel state */
    pop   {r4, r5, r6, r7, r8, r9, r10, r11, ip, lr}
    msr   psr_nzcvq, ip /* Restore psr from the ip */

    /* Exception return (back to the kernel) */
    bx    lr
ENDPROC(PendSV_Handler)

ENTRY(SVC_Handler)
    /*Note: ISR always uses msp */

    /* Disable interrupts */
    cpsid i
    cpsid f

    /* Save user state */
    mrs   r0, psp /* Load psp to the r0 */

    /* Save FPU state if required */
    tst      r14, #0x10
    it       eq
    vstmdbeq r0!, {s16-s31}

    stmdb r0!, {r7} /* Preserve syscall number */
    stmdb r0!, {r4, r5, r6, r7, r8, r9, r10, r11, lr} /* Preserve user state */

    /* Turn on the privilege mode */
    mov  r4, #0
    msr  control, r4

    /* Load kernel state */
    pop   {r4, r5, r6, r7, r8, r9, r10, r11, ip, lr}
    msr   psr_nzcvq, ip /* Load psr from the ip */

    /* Exception return (back to the kernel) */
    bx    lr
ENDPROC(SVC_Handler)

ENTRY(jump_to_thread)
    /* Arguments:
     * r0 (input) : Stack address of the thread 
     * r1 (input) : Run user thread with priviledge or not
     * r0 (return): Stack address after loading the user stack */

    /* Save kernel state */
    mrs   ip, psr /* Save psr to the ip */
    push  {r4, r5, r6, r7, r8, r9, r10, r11, ip, lr} /* Preserve kernel state */

    /* Set thread's privilege */
    msr  control, r1

    /* Load user state */
    ldmia r0!, {r4, r5, r6, r7, r8, r9, r10, r11, lr}

    /* Load syscall number */
    ldmia r0!, {r7}

    /* Load FPU state if required */
    tst      r14, #0x10
    it       eq
    vldmiaeq r0!, {s16-s31}

    msr   psp, r0 /* psp = r0 */

    /* Enable interrupts */
    cpsie i
    cpsie f

    /* Jump to user space */
    bx    lr
ENDPROC(jump_to_thread)

ENTRY(os_env_init)
    /* Arguments:
     * r0 (input): Stack address */

    /* Save kernel state */
    mrs  ip, psr /* Save psr to the ip */
    push {r4, r5, r6, r7, r8, r9, r10, r11, ip, lr}

    /* Switch stack pointer from msp to psp */
    msr  psp, r0     /* psp = r1 */
    mov  r0, #3      /* r0 = 3 */
    msr  control, r0 /* control = r0 (stack pointer is now switched to psp) */
    isb              /* Flush the pipeline as the stack pointer is changed */

    /* Switch to handler mode */
    push {r7}   /* Preserve old r7 for overwriting */
    mov  r7, #0 /* Write syscall number to the r7 */
    svc  0      /* Trigger SVC interrupt handler */
    pop  {r7}   /* Resume old r7 value */

    /* Disable interrupts */
    cpsid i 
    cpsid f

    /* Function return */
    bx lr
ENDPROC(os_env_init)
