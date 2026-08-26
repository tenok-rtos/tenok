/**
 * @file
 */
#ifndef __KERNEL_ERRNO_H__
#define __KERNEL_ERRNO_H__

#include <errno.h>

#define ERESTARTSYS 512 /**< Syscall requires restart */

/* The kernel answers a failure with the negation of its error number, POSIX
 * asks for minus one with the number left in errno
 */
static inline long set_errno(long retval)
{
    if (retval < 0) {
        errno = -retval;
        return -1;
    }

    return retval;
}

#endif
