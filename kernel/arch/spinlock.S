#include <common/linkage.h>

.syntax unified

/* lock is implemented by the load/store exclusive mechanism */
ENTRY(spinlock)
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
ENDPROC(spinlock)

/* unlock is easier which requires only reseting the lock variable to zero */
ENTRY(spin_unlock)
//arguments:
//r0 (input): address of the lock variable
        mov   r1, #0       //r1 = 0
        str   r1, [r0]     //[r0] = r1

        bx    lr           //function return
ENDPROC(spin_unlock)
