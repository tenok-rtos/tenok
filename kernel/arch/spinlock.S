#include <common/linkage.h>

.syntax unified

/* Spinlock is implemented with ARM load/store exclusive instructions */
ENTRY(spinlock)
/* Arguments:
 * r0 (input): Address of the lock variable */
loop:   ldrex r2, [r0]     /* Assign *lock value to r2 */
        cmp   r2, #1       /* Check if r2 equals 1 */
        beq   loop         /* If true then jump to loop */

        mov   r1, #1       /* Assign 1 to r1 */
        strex r2, r1, [r0] /* [r0] = r1, r2 = strex result (success:0, failed:1) */
        cmp   r2, #1       /* Check if r2 equals 1 */
        beq   loop         /* If true then jump to loop */

        bx    lr           /* Function return */
ENDPROC(spinlock)

/* Unlock is fairly easy as it only requires to reset the lock variable */
ENTRY(spin_unlock)
/* Arguments:
 * r0 (input): Address of the lock variable */
        mov   r1, #0       /* Assign 0 to r1 */
        str   r1, [r0]     /* Write r1 to the lock variable ([r0]) */

        bx    lr           /* Function return */
ENDPROC(spin_unlock)
