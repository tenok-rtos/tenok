.syntax unified

.macro syscall syscall_num
	push {r7}             //preserve old r7 for overwriting
	mov  r7, \syscall_num //write syscall number to r7 as new value
	svc  0                //trigger svc interrupt handler
	nop
	pop  {r7}             //resume old r7 value
	bx lr                 //return
.endm

.type   set_irq, %function
.global set_irq
set_irq:
	syscall #1

.type   set_program_name, %function
.global set_program_name
set_program_name:
	syscall #2

.type   sched_yield, %function
.global sched_yield
sched_yield:
	syscall #3

.type   fork, %function
.global fork
fork:
	syscall #4

.type   sleep, %function
.global sleep
sleep:
	syscall #5

.type   mount, %function
.global mount
mount:
	syscall #6

.type   open, %function
.global open
open:
	syscall #7

.type   close, %function
.global close
close:
        syscall #8

.type   read, %function
.global read
read:
	syscall #9

.type   write, %function
.global write
write:
	syscall #10

.type   lseek, %function
.global lseek
lseek:
	syscall #11

.type   fstat, %function
.global fstat
fstat:
	syscall #12

.type   opendir, %function
.global opendir
opendir:
	syscall #13

.type   readdir, %function
.global readdir
readdir:
	syscall #14

.type   getpriority, %function
.global getpriority
getpriority:
	syscall #15

.type   setpriority, %function
.global setpriority
setpriority:
	syscall #16

.type   getpid, %function
.global getpid
getpid:
	syscall #17

.type   mknod, %function
.global mknod
mknod:
	syscall #18

.type   mkfifo, %function
.global mkfifo
mkfifo:
	syscall #19

.type   mq_open, %function
.global mq_open
mq_open:
        syscall #20

.type   mq_receive, %function
.global mq_receive
mq_receive:
        syscall #21

.type   mq_send, %function
.global mq_send
mq_send:
        syscall #22

.type   pthread_mutex_init, %function
.global pthread_mutex_init
pthread_mutex_init:
        syscall #23

.type   pthread_mutex_unlock, %function
.global pthread_mutex_unlock
pthread_mutex_unlock:
        syscall #24

.type   pthread_mutex_lock, %function
.global pthread_mutex_lock
pthread_mutex_lock:
        syscall #25

.type   pthread_cond_init, %function
.global pthread_cond_init
pthread_cond_init:
        syscall #26

.type   pthread_cond_signal, %function
.global pthread_cond_signal
pthread_cond_signal:
        syscall #27

.type   pthread_cond_wait, %function
.global pthread_cond_wait
pthread_cond_wait:
        syscall #28

.type   sem_init, %function
.global sem_init
sem_init:
        syscall #29

.type   sem_post, %function
.global sem_post
sem_post:
        syscall #30

.type   sem_trywait, %function
.global sem_trywait
sem_trywait:
        syscall #31

.type   sem_wait, %function
.global sem_wait
sem_wait:
        syscall #32

.type   sem_getvalue, %function
.global sem_getvalue
sem_getvalue:
        syscall #33
