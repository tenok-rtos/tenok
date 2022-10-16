.syntax unified

.type	SysTick_Handler, %function
.global SysTick_Handler

.type	jump_to_user_space, %function
.global jump_to_user_space

SysTick_Handler:
	/* the cpu uses the msp as the stack pointer since we are
         * now in the interrupt handler */

	cpsid i //disable all interrupts

	/* save user state */
	mrs r0, psp //r0 = psp (for saving user space context in the psp)
	stmdb r0!, {r4, r5, r6, r7, r8, r9, r10, r11, lr}

	/* load kernel state */
	pop {r4, r5, r6, r7, r8, r9, r10, r11, ip, lr} //load r4-r11, ip and lr from msp
	msr psr_nzcvq, ip //psr = ip
	orr lr, #0x0d 
	bx lr //jump to the kernel space

jump_to_user_space:
	//r0 = &next_tcb (function argument)

	/* save kernel state */
	mrs ip, psr //ip = psr
	push {r4, r5, r6, r7, r8, r9, r10, r11, ip, lr} //save r4-r11, ip and lr to msp

	/* load user state */
	ldr r1, [r0]    //r1 = next_tcb->top_of_the_stack
	msr psp, r1     //psp = r1
	mov r0, #3      //r0 = 3
	msr control, r0 //control = r0 (stack pointer is now switched to psp)
	pop {r4, r5, r6, r7, r8, r9, r10, r11, lr} //load r4-r11 and lr from psp
	cpsie i         //enable all interrupts
	bx r14 //jump to the user space
