/**
 * @file
 */
#ifndef __IOCTL_H__
#define __IOCTL_H__

/**
 * @brief  Perform device-specific control
 * @param  fd: The file descriptor number of the file.
 * @param  request: The request command to perform.
 * @param  arg: The argument the request takes, which a request that takes
 *         none does not mind being handed.
 * @retval int: 0 on success and -1 on error, with the reason left in errno.
 */
int ioctl(int fd, unsigned long request, ...);

#endif
