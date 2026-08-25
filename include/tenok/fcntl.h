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

/* Commands of fcntl(), with the numbers Linux gives them */
#define F_DUPFD 0
#define F_GETFD 1
#define F_SETFD 2
#define F_GETFL 3
#define F_SETFL 4
#define F_DUPFD_CLOEXEC 1030

/* Tenok has no exec() for this to act on, it is stored and read back */
#define FD_CLOEXEC 1

/**
 * @brief  Open the file specified by the pathname
 * @param  pathname: The pathname of the file.
 * @param  flags: Flags for opening the file.
 * @param  mode: The permission bits of the file, when O_CREAT is given and
 *         the file has to be created. Ignored otherwise.
 * @retval int: The file descriptor of the file on success and -1 on error,
 *         with the reason left in errno.
 */
int open(const char *pathname, int flags, ...);

/**
 * @brief  Act on an open file descriptor
 * @param  fd: The file descriptor to provide.
 * @param  cmd: F_DUPFD, F_GETFD, F_SETFD, F_GETFL or F_SETFL.
 * @param  arg: The lowest descriptor to duplicate onto, or the flags to set.
 * @retval int: The answer of the command on success and -1 on error, with the
 *         reason left in errno.
 */
int fcntl(int fd, int cmd, ...);

#endif
