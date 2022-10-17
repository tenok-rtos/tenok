.syntax unified
  
.type   fork, %function
.global fork

.type   sleep, %function
.global sleep

.type getpriority, %function
.global getpriority

.type getpid, %function
.global getpid

.macro syscall syscall_num
	push {r7}            //preserve old r7 for overwriting
	mov r7, \syscall_num //write syscall number to r7 as new value
	svc 0                //trigger svc interrupt handler
	nop
	pop {r7}             //resume old r7 value
	bx lr                //return
.endm

fork:
	syscall #1

sleep:
	syscall #2

getpriority:
	syscall #7

getpid:
	syscall #9
