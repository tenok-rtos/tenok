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
