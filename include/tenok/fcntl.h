/**
 * @file
 */
#ifndef __FCNTL_H__
#define __FCNTL_H__

#define O_RDONLY 0
#define O_WRONLY 1
#define O_RDWR 2
#define O_CREAT 0x0200
#define O_EXCL 0x0800
#define O_NONBLOCK 00004000

/**
 * @brief  Open the file specified by the pathname
 * @param  pathname: The pathname of the file.
 * @param  flags: Flags for opening the file.
 * @retval int: The function returns file descriptor number of the
 *         file on success and nonzero error number on error.
 */
int open(const char *pathname, int flags);

#endif
