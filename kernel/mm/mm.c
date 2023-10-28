#include <stddef.h>

#include <arch/port.h>
#include <kernel/syscall.h>

NACKED void *malloc(size_t size)
{
    SYSCALL(MALLOC);
}

NACKED void free(void *ptr)
{
    SYSCALL(FREE);
}

/* not implemented. the function is defined only
 * to supress the newlib warning
 */
void *_sbrk(int incr)
{
    return NULL;
}
