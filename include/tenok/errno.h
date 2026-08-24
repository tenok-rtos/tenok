/**
 * @file
 */
#ifndef __ERRNO_H__
#define __ERRNO_H__

/* The system calls report a failure by returning a negative error number, but
 * the standard library reports one through this variable, and that is where a
 * program that was not written for Tenok looks.
 *
 * It lives in the reentrancy structure of the C library, of which Tenok keeps
 * a single one: two threads that fail at the same time overwrite each other.
 */
#ifndef errno
extern int *__errno(void);
#define errno (*__errno())
#endif

#define EPERM 1         /**< Not the owner */
#define ENOENT 2        /**< No such file or directory */
#define ESRCH 3         /**< No such task */
#define EINTR 4         /**< Syscall is interrupted */
#define EIO 5           /**< I/O error */
#define ENXIO 6         /**< No such device or address */
#define E2BIG 7         /**< Argument list too long */
#define EBADF 9         /**< Bad file descriptor number */
#define EAGAIN 11       /**< Try again */
#define ENOMEM 12       /**< Not enough memory */
#define EACCES 13       /**< Permission denied */
#define EFAULT 14       /**< Bad address */
#define ENOTBLK 15      /**< Not a block device */
#define EBUSY 16        /**< Device or resource busy */
#define EEXIST 17       /**< File exists */
#define EXDEV 18        /**< Cross-device link */
#define ENODEV 19       /**< No such device */
#define ENOTDIR 20      /**< Not a directory */
#define EISDIR 21       /**< Is a directory */
#define EINVAL 22       /**< Invalid argument */
#define ENFILE 23       /**< Too many open files in the system */
#define EMFILE 24       /**< File descriptor value too long */
#define ENOTTY 25       /**< Not a character device */
#define ETXTBSY 26      /**< Text file busy */
#define EFBIG 27        /**< File is too big */
#define ENOSPC 28       /**< No space left */
#define ESPIPE 29       /**< Illegal seek */
#define EROFS 30        /**< Read-only file system */
#define EDEADLK 45      /**< Deadlock */
#define ENOSYS 88       /**< Function not implemented */
#define ENOTEMPTY 90    /**< Directory not empty */
#define ENAMETOOLONG 91 /**< File or path name too long */
#define EMSGSIZE 122    /**< Message to long */
#define ETIMEDOUT 110   /**< Connection timed out */
#define EOVERFLOW 139   /**< Numerical overflow */

/* Numbers Tenok never returns of its own. They are named so that a program
 * written for a POSIX system compiles, and they follow newlib, which is the
 * library the numbers have to agree with
 */
#define ENOEXEC 8   /**< Exec format error */
#define ECHILD 10   /**< No child processes */
#define ERANGE 34   /**< Result too large */
#define ELOOP 92    /**< Too many levels of symbolic links */
#define ENOTSUP 134 /**< Not supported */

#endif
