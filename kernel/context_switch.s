.syntax unified

.type	SysTick_Handler, %function
.global SysTick_Handler

.type	SVC_Handler, %function
.global SVC_Handler

.type   os_env_init, %function
.global os_env_init

.type	jump_to_user_space, %function
.global jump_to_user_space

SysTick_Handler:
	/* handler mode: cpu now uses msp as the stack pointer */

	/* disable all interrupts before entering into the kernel */
	//cpsid i
	mov   r0,      #0x50
	msr   basepri, r0

	neg   r7,  r7 //negate _r7 to indicate the kernel is going to be returned by the systick irq

	/* save user state */
	mrs   r0,  psp  //load psp into the r0
	stmdb r0!, {r7} //save _r7 for the syscall number
	stmdb r0!, {r4, r5, r6, r7, r8, r9, r10, r11, lr} //save user context of r4-r11 and lr

	/* load kernel state */
	pop   {r4, r5, r6, r7, r8, r9, r10, r11, ip, lr} //load r4-r11, ip and lr from the msp
	msr   psr_nzcvq, ip //load psr from the ip

	/* exception return */
	bx    lr //jump to the kernel space

SVC_Handler:
	/* handler mode: cpu now uses msp as the stack pointer */

	/* disable all interrupts before entering into the kernel */
	//cpsid i
	mov  r0,      #0x50
	msr  basepri, r0

	/* save user state */
	mrs   r0, psp   //load psp into the r0
	stmdb r0!, {r7} //save _r7 for the syscall number
	stmdb r0!, {r4, r5, r6, r7, r8, r9, r10, r11, lr} //save user context of r4-r11 and lr

	/* load kernel state */
	pop   {r4, r5, r6, r7, r8, r9, r10, r11, ip, lr}  //load r4-r11, ip and lr from the msp
	msr   psr_nzcvq, ip //load psr from the ip

	/* exception return */
	bx    lr //jump to the kernel space

jump_to_user_space:
	//r0 = &stack_address (function argument and return value)
	//r1 = irq_disabled (indicate the irq is disabled by the syscall or not)
	//the function is designed to called by the kernel, i.e., msp is now selected as the stack pointer

	/* save kernel state */
	mrs   ip, psr   //save psr into the ip
	push  {r4, r5, r6, r7, r8, r9, r10, r11, ip, lr}  //save kernel context r4-r11, ip and lr

	/* load user state */
	ldmia r0!, {r4, r5, r6, r7, r8, r9, r10, r11, lr} //load the user state from the address pointed by the r0
	ldmia r0!, {r7} //load syscall number _r7
	msr   psp, r0   //psp = r0 (swap the psp to the address pointed by r0)

	/* enable all interrupts */
	push  {r0, r1, r2}
	mov   r2, #1    //r2 = 1
	cmp   r1, r2    //if(irq_disabled == 1)
	beq   return    //return to user space without turn on all the interrupts

	//cpsie i 
	mov   r0,      #0
	msr   basepri, r0

return: pop {r0, r1, r2}

	bx    lr

os_env_init:
	//r0 = stack_address (function argument)

	/* save kernel state */
	mrs  ip, psr //save psr into the ip
	push {r4, r5, r6, r7, r8, r9, r10, r11, ip, lr} //save r4-r11, ip and lr to msp

	/* switch stack pointer from msp to psp */
	msr  psp, r0     //psp = r1
	mov  r0, #3      //r0 = 3
	msr  control, r0 //control = r0 (stack pointer is now switched to psp)
	isb              //flush the cpu pipeline

	/* switch to handler mode via svc */
        push {r7}   //preserve old r7 for overwriting
        mov  r7, #0 //write syscall number to r7 as new value
        svc  0      //trigger svc interrupt handler
        nop
        pop  {r7}   //resume old r7 value

	/* enter into the kernel, disable all interrupts */
	//cpsid i 
	mov  r0,      #0x50
	msr  basepri, r0

	/* return */
	bx lr
