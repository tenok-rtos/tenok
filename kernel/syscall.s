.syntax unified

.macro syscall syscall_num
	push {r7}             //preserve old r7 for overwriting
	mov  r7, \syscall_num //write syscall number to r7 as new value
	svc  0                //trigger svc interrupt handler
	nop
	pop  {r7}             //resume old r7 value
	bx lr                 //return
.endm

.type   yield, %function
.global yield
yield:
	syscall #0

.type   set_irq, %function
.global set_irq
set_irq:
	syscall #1

.type   fork, %function
.global fork
fork:
	syscall #2

.type   sleep, %function
.global sleep
sleep:
	syscall #3

.type   read, %function
.global read
read:
	syscall #6

.type   write, %function
.global write
write:
	syscall #7

.type   getpriority, %function
.global getpriority
getpriority:
	syscall #8

.type   setpriority, %function
.global setpriority
setpriority:
	syscall #9

.type   getpid, %function
.global getpid
getpid:
	syscall #10

.type   mknod, %function
.global mknod
mknod:
	syscall #11
