#include <common/linkage.h>

/* NVIC Priority Group 4:
 * Group priority bits: PRI_M[7:4]
 * Subpriority bits: None
 * By setting BASEPRI to 16 (1 << 4), all interrupts with priority value
 * lower than 0 will be disable (Lower priority value has higher priority).
 */
#define PRI_THRESHOLD #16

.syntax unified

.macro set_basepri
    push {r0}
    mov r0, PRI_THRESHOLD
    msr basepri, r0
    pop {r0}
.endm

.macro reset_basepri
    push {r0}
    mov r0, #0 
    msr basepri, r0
    pop {r0}
.endm

.macro irq_disable
    set_basepri
    push {lr}
    bl preempt_count_inc
    pop {lr}
.endm

.macro irq_enable
    push {lr}
    bl preempt_count_dec
    pop {lr}
    reset_basepri
.endm

ENTRY(PendSV_Handler)
    /* Disable interrupts */
    irq_disable

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
    pop  {r4, r5, r6, r7, r8, r9, r10, r11, ip, lr}
    msr  psr_nzcvq, ip /* Load psr from the ip */

    /* Set syscall request flag */
    push {r0-r3, lr}
    bl   set_syscall_flag 
    pop  {r0-r3, lr}

    /* Exception return (back to the kernel) */
    bx   lr
ENDPROC(SVC_Handler)

ENTRY(__preempt_disable)
    set_basepri
    bx lr
ENDPROC(__preempt_disable)

ENTRY(__preempt_enable)
    reset_basepri
    bx lr
ENDPROC(__preempt_enable)

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
