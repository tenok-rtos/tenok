#include <common/linkage.h>

#define PRI_THRESHOLD #1

.syntax unified

.macro irq_disable
    push {r0}
    mov r0, PRI_THRESHOLD /* Set priority threshold to 1 */
    msr basepri, r0
    pop {r0}
.endm

.macro irq_enable
    push {r0}
    mov r0, #0 /* No priority threshold is set */ 
    msr basepri, r0
    pop {r0}
.endm

ENTRY(PendSV_Handler)
    /* Disable interrupts */
    irq_disable

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
    /* Disable interrupts */
    irq_disable

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

ENTRY(preempt_disable)
    irq_disable
    bx lr
ENDPROC(preempt_disable)

ENTRY(preempt_enable)
    irq_enable
    bx lr
ENDPROC(preempt_enable)

ENTRY(jump_to_thread)
    /* Arguments:
     * r0 (input) : Stack address of the thread 
     * r1 (input) : Run user thread with priviledge or not
     * r0 (return): Stack address after loading the user stack
     */

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
    irq_enable

    /* Jump to user space */
    bx    lr
ENDPROC(jump_to_thread)

ENTRY(syscall)
    /* Arguments:
     * r0 (input): Function pointer to the syscall handler 
     * r1 (input): Pointers to the syscall arguments
     * r2 (input): Pointer to the syscall pending flag
     */

    push {r4, r5, r6, r7, lr}
    mov r4, r0
    mov r5, r1
    mov r6, r2

    /* Load first syscall argument */
    ldr r0, [r5, #0]
    mov r7, r0 /* Preserve address of r0 in thread stack */
    ldr r0, [r0]

    /* Load second syscall argument */
    ldr r1, [r5, #4]
    ldr r1, [r1]

    /* Load third syscall argument */
    ldr r2, [r5, #8]
    ldr r2, [r2]

    /* Load fourth syscall argument */
    ldr r3, [r5, #12]
    ldr r3, [r3]

    blx r4       /* Jump to the syscall handler */
    mov r5, r0   /* Store syscall return value to r5 */

    /* Save syscall retval back to r0 in thread stack
     * if the pending flag is reset */
    ldr r6, [r6]
    cmp r6, #0
    it  eq 
    streq r5, [r7] /* Save return value to r0 in thread stack */

    pop {r4, r5, r6, r7, lr}

    /* Function return */
    bx lr
ENDPROC(syscall)

ENTRY(os_env_init)
    /* Arguments:
     * r0 (input): Stack address
     */

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
    irq_disable

    /* Function return */
    bx lr
ENDPROC(os_env_init)

/* Spinlock is implemented with ARM load/store exclusive instructions */
ENTRY(spinlock)
    /* Arguments:
     * r0 (input): Address of the lock variable
     */

loop:
    ldrex r2, [r0]     /* Assign *lock value to r2 */
    cmp   r2, #1       /* Check if r2 equals 1 */
    beq   loop         /* If true then jump to loop */

    mov   r1, #1       /* Assign 1 to r1 */
    strex r2, r1, [r0] /* [r0] = r1, r2 = strex result (success:0, failed:1) */
    cmp   r2, #1       /* Check if r2 equals 1 */
    beq   loop         /* If true then jump to loop */

    bx    lr           /* Function return */
ENDPROC(spinlock)

/* Unlock is fairly easy as it only requires to reset the lock variable */
ENTRY(spin_unlock)
    /* Arguments:
     * r0 (input): Address of the lock variable
     */

    mov   r1, #0       /* Assign 0 to r1 */
    str   r1, [r0]     /* Write r1 to the lock variable ([r0]) */

    bx    lr           /* Function return */
ENDPROC(spin_unlock)
