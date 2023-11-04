/**
 * @file
 */
#ifndef __IOCTL_H__
#define __IOCTL_H__

/**
 * @brief  Perform device-specific control
 * @param  fd: The file descriptor number of the file.
 * @param  request: The request command to perform.
 * @param  arg: The argument to pass with the request.
 * @retval int: 0 on sucess and nonzero error number on error.
 */
int ioctl(int fd, unsigned int request, unsigned long arg);

#endif
