/**
 * @file
 */
#ifndef  __FCNTL_H__
#define  __FCNTL_H__

#define O_RDONLY   0
#define O_WRONLY   1
#define O_RDWR     2
#define O_CREAT    0x0200
#define O_EXCL     0x0800
#define O_NONBLOCK 00004000

/**
 * @brief  Open the file specified by pathname
 * @param  pathname: The pathname of the file to provide.
 * @param  flags: Flags for opening the file. Currently only support O_NONBLOCK.
 * @retval int: File descriptor number of the file on success and nonzero error
                number on error.
 */
int open(const char *pathname, int flags);

#endif
