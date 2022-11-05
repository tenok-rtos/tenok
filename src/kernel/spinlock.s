.syntax unified

.type   spin_lock, %function
.global spin_lock
spin_lock:
loop:   ldrex r2, [r0]     //r2 = *lock
        cmp   r2, #1       //if(r2 == 1)
        beq   loop         //goto loop

        mov   r1, #1       //r0 = 1
        strex r2, r1, [r0] //[r0] = r1, r2 = result (success:0, failed:1)
        cmp   r2, #1       //if(r2 == 1)
        beq   loop         //goto loop

        bx    lr           //return

/* unlock is easier since lock operation is exclusive and by writing
 *  zero, the lock will be release */
.type   spin_unlock, %function
.global spin_unlock
spin_unlock:
	mov r1, #0
	str r1, [r0]
