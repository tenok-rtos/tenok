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
	syscall #1

.type   set_irq, %function
.global set_irq
set_irq:
	syscall #2

.type   fork, %function
.global fork
fork:
	syscall #3

.type   sleep, %function
.global sleep
sleep:
	syscall #4

.type   open, %function
.global open
open:
	syscall #5

.type   close, %function
.global close
close:
	syscall #6

.type   read, %function
.global read
read:
	syscall #7

.type   write, %function
.global write
write:
	syscall #8

.type   getpriority, %function
.global getpriority
getpriority:
	syscall #9

.type   setpriority, %function
.global setpriority
setpriority:
	syscall #10

.type   getpid, %function
.global getpid
getpid:
	syscall #11

.type   mknod, %function
.global mknod
mknod:
	syscall #12

.type   mkdir, %function
.global mkdir
mkdir:
        syscall #13

.type   rmdir, %function
.global rmdir
rmdir:
        syscall #14
