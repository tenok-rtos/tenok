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
	/* the cpu uses the msp as the stack pointer since we are
         * now in a interrupt handler */

	/* enter into the kernel, disable all interrupts */
	cpsid i

	mov r7, 0 //set syscall number to zero

	/* save user state */
	mrs r0, psp     //r0 = psp (for saving user space context in the psp)
	stmdb r0!, {r7} //save syscall number _r7
	stmdb r0!, {r4, r5, r6, r7, r8, r9, r10, r11, lr} //save the state to the address pointed by the r0

	/* load kernel state */
	pop {r4, r5, r6, r7, r8, r9, r10, r11, ip, lr} //load r4-r11, ip and lr from msp
	msr psr_nzcvq, ip //psr = ip

	/* exception return */
	bx lr //jump back to the kernel space

SVC_Handler:
	/* the cpu uses the msp as the stack pointer since we are
         * now in a interrupt handler */

	/* enter into the kernel, disable all interrupts */
	cpsid i

	/* save user state */
	mrs r0, psp //r0 = psp
	stmdb r0!, {r7} //save syscall number _r7
	stmdb r0!, {r4, r5, r6, r7, r8, r9, r10, r11, lr} //save the state to the address pointed by the r0

	/* load kernel state */
	pop {r4, r5, r6, r7, r8, r9, r10, r11, ip, lr} //load r4-r11, ip and lr from msp
	msr psr_nzcvq, ip //psr = ip

	/* exception return */
	bx lr //jump back to the kernel space

jump_to_user_space:
	//r0 = &stack_address (function argument and return value)

	/* save kernel state */
	mrs ip, psr //ip = psr
	push {r4, r5, r6, r7, r8, r9, r10, r11, ip, lr} //save r4-r11, ip and lr to msp

	/* load user state */
	ldmia r0!, {r4, r5, r6, r7, r8, r9, r10, r11, lr} //load the state from the address pointed by the r0
	ldmia r0!, {r7} //load syscall number _r7
	msr psp, r0     //psp = r0 (swap the psp to the address pointed by r0)
	cpsie i         //enable all interrupts
	bx lr           //jump to the user space

os_env_init:
	//r0 = stack_address (function argument)

	/* save kernel state */
	mrs ip, psr //ip = psr
	push {r4, r5, r6, r7, r8, r9, r10, r11, ip, lr} //save r4-r11, ip and lr to msp

	/* switch stack pointer from msp to psp */
	msr psp, r0     //psp = r1
	mov r0, #3      //r0 = 3
	msr control, r0 //control = r0 (stack pointer is now switched to psp)
	isb             //flush the cpu pipeline

	/* switch to handler mode via svc */
        push {r7} //preserve old r7 for overwriting
        mov r7, 0 //write syscall number to r7 as new value
        svc 0     //trigger svc interrupt handler
        nop
        pop {r7}  //resume old r7 value

	/* enter into the kernel, disable all interrupts */
	cpsid i 

	/* return */
	bx lr
