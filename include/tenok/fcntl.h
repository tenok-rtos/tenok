/**
 * @file
 */
#ifndef __FCNTL_H__
#define __FCNTL_H__

#define O_ACCMODE 0x0003
#define O_RDONLY 0
#define O_WRONLY 1
#define O_RDWR 2
#define O_APPEND 0x0008
#define O_CREAT 0x0200
#define O_TRUNC 0x0400
#define O_EXCL 0x0800
#define O_NONBLOCK 0x4000

/**
 * @brief  Open the file specified by the pathname
 * @param  pathname: The pathname of the file.
 * @param  flags: Flags for opening the file.
 * @retval int: The file descriptor of the file on success and -1 on error,
 *         with the reason left in errno.
 */
int open(const char *pathname, int flags);

#endif
