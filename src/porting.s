.syntax unified

.type   sem_up, %function
.global sem_up
sem_up:
	//[arguments] r0: write object, r1: new value

	ldrex r2, [r0]
	cmp   r2, #1
	beq   up_failed

	strex r2, r1, [r0] //write r1 to [r0] and store result in r2
	cmp   r2, #1
	beq   up_failed

	mov   r0, #0
	bx    lr

up_failed:
	mov   r0, #1
	bx    lr

.type   sem_down, %function
.global sem_down
sem_down:
	//[arguments] r0: write object, r1: new value

	ldrex r2, [r0]
	cmp   r2, #0
	beq   down_failed

	strex r2, r1, [r0] //write r1 to [r0] and store result in r2
	cmp   r2, #1
	beq   down_failed

	mov r0, #0
	bx lr

down_failed:
	mov r0, #1
	bx lr
