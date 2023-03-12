.syntax unified

.type   SysTick_Handler, %function
.global SysTick_Handler
SysTick_Handler:
    /* note: ISRs are executed in the handler mode which use msp as the stack pointer */

    /* disable all interrupts before entering into the kernel */
    cpsid i
    cpsid f
    
    neg   r7,  r7 //negate _r7 to indicate that the kernel is returned from the systick ISR

    /* save user state */
    mrs   r0,  psp  //load psp into the r0

    tst r14, #0x10
    it eq
    vstmdbeq r0!, {s16-s31}

    stmdb r0!, {r7} //preserve the syscall number of the _r7
    stmdb r0!, {r4, r5, r6, r7, r8, r9, r10, r11, lr} //preserve context: r4-r11 and lr

    /* load kernel state */
    pop   {r4, r5, r6, r7, r8, r9, r10, r11, ip, lr} //load r4-r11, ip and lr from the msp
    msr   psr_nzcvq, ip //load psr from the ip

    /* exception return */
    bx    lr //jump to the kernel space

.type   SVC_Handler, %function
.global SVC_Handler
SVC_Handler:
    /* note: ISRs are executed in the handler mode which use msp as the stack pointer */

    /* disable the interrupt */
    cpsid i
    cpsid f

    /* save user state */
    mrs   r0, psp   //load psp into the r0

    tst r14, #0x10
    it eq
    vstmdbeq r0!, {s16-s31}

    stmdb r0!, {r7} //preserve the syscall number of the _r7
    stmdb r0!, {r4, r5, r6, r7, r8, r9, r10, r11, lr} //preserve user context: r4-r11 and lr

    /* load kernel state */
    pop   {r4, r5, r6, r7, r8, r9, r10, r11, ip, lr}  //load r4-r11, ip and lr from the msp
    msr   psr_nzcvq, ip //load psr from the ip

    /* exception return */
    bx    lr //jump to the kernel space

.type   jump_to_user_space, %function
.global jump_to_user_space
jump_to_user_space:
    //arguments:
    //r0 (input) : stack address of the user task
    //r0 (return): stack address after loading the user stack

    /* save kernel state */
    mrs   ip, psr //save psr into the ip
    push  {r4, r5, r6, r7, r8, r9, r10, r11, ip, lr} //preserve kernel context: r4-r11, ip and lr

    /* load user state */
    ldmia r0!, {r4, r5, r6, r7, r8, r9, r10, r11, lr} //load the user state from the address pointed by the r0
    ldmia r0!, {r7} //load syscall number _r7

    tst r14, #0x10
    it eq
    vldmiaeq r0!, {s16-s31}

    msr   psp, r0   //psp = r0 (set the psp to the stack address of the task to jump)

    /* enable the interrupt */
    cpsie i
    cpsie f

    bx    lr //jump to the user task

.type   os_env_init, %function
.global os_env_init
os_env_init:
    //arguments:
    //r0 (input): stack address

    /* save kernel state */
    mrs  ip, psr //save psr into the ip
    push {r4, r5, r6, r7, r8, r9, r10, r11, ip, lr} //preserve r4-r11, ip, and lr to the space pointed by msp

    /* switch stack pointer from msp to psp */
    msr  psp, r0     //psp = r1
    mov  r0, #3      //r0 = 3
    msr  control, r0 //control = r0 (stack pointer is now switched to psp)
    isb              //flush the cpu pipeline

    /* switch to handler mode via svc */
    push {r7}   //preserve old r7 for overwriting
    mov  r7, #0 //write syscall number to the r7
    svc  0      //trigger svc interrupt handler
    nop
    pop  {r7}   //resume old r7 value

    /* disable the interrupt before entering into the kernel */
    cpsid i 
    cpsid f

    bx lr //function return
