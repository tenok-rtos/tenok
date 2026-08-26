/**
 * @file
 */
#ifndef __KERNEL_ERRNO_H__
#define __KERNEL_ERRNO_H__

#include <errno.h>

#define ERESTARTSYS 512 /**< Syscall requires restart */

/* The kernel negates its error number, POSIX leaves it in errno */
static inline long set_errno(long retval)
{
    if (retval < 0) {
        errno = -retval;
        return -1;
    }

    return retval;
}

#endif
