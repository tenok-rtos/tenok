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

.type   mq_open, %function
.global mq_open
mq_open:
	syscall #15

.type   mq_send, %function
.global mq_send
mq_send:
	syscall #16

.type   mq_receive, %function
.global mq_receive
mq_receive:
	syscall #17

.type   mq_close, %function
.global mq_close
mq_close:
	syscall #18
