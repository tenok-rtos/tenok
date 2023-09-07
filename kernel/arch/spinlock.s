.syntax unified

/* lock is implemented by the load/store exclusive mechanism */
.type   spin_lock, %function
.global spin_lock
spin_lock:
//arguments:
//r0 (input): address of the lock variable
loop:   ldrex r2, [r0]     //r2 = *lock
        cmp   r2, #1       //if(r2 == 1)
        beq   loop         //    goto loop

        mov   r1, #1       //r1 = 1
        strex r2, r1, [r0] //[r0] = r1, r2 = strex result (success:0, failed:1)
        cmp   r2, #1       //if(r2 == 1)
        beq   loop         //    goto loop

        bx    lr           //function return

/* unlock is easier which requires only reseting the lock variable to zero */
.type   spin_unlock, %function
.global spin_unlock
//arguments:
//r0 (input): address of the lock variable
spin_unlock:
        mov   r1, #0       //r1 = 0
        str   r1, [r0]     //[r0] = r1

        bx    lr           //function return
