/**
 * @file
 */
#ifndef  __FCNTL_H__
#define  __FCNTL_H__

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
