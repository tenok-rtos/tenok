#include <kernel/linkage.h>

.syntax unified

ENTRY(PendSV_Handler)
    /* note: ISRs are executed in the handler mode which use msp as the stack pointer */

    /* disable all interrupts before entering into the kernel */
    cpsid i
    cpsid f
    
    neg   r7,  r7 //negate _r7 to indicate that the kernel is returned from pendsv handler 

    /* save user state */
    mrs   r0,  psp  //load psp into the r0

    tst      r14, #0x10
    it       eq
    vstmdbeq r0!, {s16-s31}

    stmdb r0!, {r7} //preserve the syscall number of the _r7
    stmdb r0!, {r4, r5, r6, r7, r8, r9, r10, r11, lr} //preserve context: r4-r11 and lr

    /* set thread mode to be privileged */
    mov  r4, #0
    msr  control, r4

    /* load kernel state */
    pop   {r4, r5, r6, r7, r8, r9, r10, r11, ip, lr} //load r4-r11, ip and lr from the msp
    msr   psr_nzcvq, ip //load psr from the ip

    /* exception return */
    bx    lr //jump to the kernel space
ENDPROC(PendSV_Handler)

ENTRY(SVC_Handler)
    /* note: ISRs are executed in the handler mode which use msp as the stack pointer */

    /* disable the interrupt */
    cpsid i
    cpsid f

    /* save user state */
    mrs   r0, psp   //load psp into the r0

    tst      r14, #0x10
    it       eq
    vstmdbeq r0!, {s16-s31}

    stmdb r0!, {r7} //preserve the syscall number of the _r7
    stmdb r0!, {r4, r5, r6, r7, r8, r9, r10, r11, lr} //preserve user context: r4-r11 and lr

    /* set thread mode to be privileged */
    mov  r4, #0
    msr  control, r4

    /* load kernel state */
    pop   {r4, r5, r6, r7, r8, r9, r10, r11, ip, lr}  //load r4-r11, ip and lr from the msp
    msr   psr_nzcvq, ip //load psr from the ip

    /* exception return */
    bx    lr //jump to the kernel space
ENDPROC(SVC_Handler)

ENTRY(jump_to_thread)
    //arguments:
    //r0 (input) : stack address of the thread 
    //r1 (input) : run user task with priviledge or not
    //r0 (return): stack address after loading the user stack

    /* save kernel state */
    mrs   ip, psr //save psr into the ip
    push  {r4, r5, r6, r7, r8, r9, r10, r11, ip, lr} //preserve kernel context: r4-r11, ip and lr

    /* set thread's privilege */
    msr  control, r1

    /* load user state */
    ldmia r0!, {r4, r5, r6, r7, r8, r9, r10, r11, lr} //load the user state from the address pointed by the r0
    ldmia r0!, {r7} //load syscall number _r7

    tst      r14, #0x10
    it       eq
    vldmiaeq r0!, {s16-s31}

    msr   psp, r0   //psp = r0 (set the psp to the stack address of the task to jump)

    /* enable the interrupt */
    cpsie i
    cpsie f

    bx    lr //jump to the user task
ENDPROC(jump_to_thread)

ENTRY(os_env_init)
    //arguments:
    //r0 (input): stack address

    /* save kernel state */
    mrs  ip, psr //save psr into the ip
    push {r4, r5, r6, r7, r8, r9, r10, r11, ip, lr} //preserve r4-r11, ip, and lr to the space pointed by msp

    /* switch stack pointer from msp to psp */
    msr  psp, r0     //psp = r1
    mov  r0, #3      //r0 = 3
    msr  control, r0 //control = r0 (stack pointer is now switched to psp)
    isb              //flush the pipeline since the stack pointer is changed

    /* switch to handler mode via svc */
    push {r7}   //preserve old r7 for overwriting
    mov  r7, #0 //write syscall number to the r7
    svc  0      //trigger svc interrupt handler
    pop  {r7}   //resume old r7 value

    /* disable the interrupt before entering into the kernel */
    cpsid i 
    cpsid f

    bx lr //function return
ENDPROC(os_env_init)
