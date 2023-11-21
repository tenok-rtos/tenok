/**
 * @file
 */
#ifndef __ERRNO_H__
#define __ERRNO_H__

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
#define ENOTDIR 20      /**< Not a directory */
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
#define ENAMETOOLONG 91 /**< File or path name too long */
#define EMSGSIZE 122    /**< Message to long */
#define EOVERFLOW 139   /**< Numerical overflow */

#endif
