.syntax unified

.macro syscall syscall_num
	push {r7}             //preserve old r7 for overwriting
	mov  r7, \syscall_num //write syscall number to r7 as new value
	svc  0                //trigger svc interrupt handler
	nop
	pop  {r7}             //resume old r7 value
	bx lr                 //return
.endm

.type   fork, %function
.global fork
fork:
	syscall #1

.type   sleep, %function
.global sleep
sleep:
	syscall #2

.type   getpriority, %function
.global getpriority
getpriority:
	syscall #7

.type   setpriority, %function
.global setpriority
setpriority:
	syscall #8

.type   getpid, %function
.global getpid
getpid:
	syscall #9

.type   mkdir, %function
.global mkdir
mkdir:
	syscall #10

.type   sem_init, %function
.global sem_init
rmdir:
	syscall #11

.type   sem_init, %function
.global sem_init
sem_init:
	syscall #12

.type   sem_post, %function
.global sem_post
sem_post:
	syscall #13

.type   sem_wait, %function
.global sem_wait
sem_wait:
	syscall #14
