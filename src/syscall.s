.syntax unified
  
.type   fork, %function
.global fork

.type   sleep, %function
.global sleep

.type getpriority, %function
.global getpriority

.type setpriority, %function
.global setpriority

.type getpid, %function
.global getpid

.type mkdir, %function
.global mkdir

.type rmdir, %function
.global rmdir

.type sem_init, %function
.global sem_init

.type sem_post, %function
.global sem_post

.type sem_wait, %function
.global sem_wait

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

setpriority:
	syscall #8

getpid:
	syscall #9

mkdir:
	syscall #10

rmdir:
	syscall #11

sem_init:
	syscall #12

sem_post:
	syscall #13

sem_wait:
	syscall #14
