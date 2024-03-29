/**
 * @file
 */
#ifndef __UNISTD_H__
#define __UNISTD_H__

#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

#include "kconfig.h"

#define MQ_PRIO_MAX _MQ_PRIO_MAX

#define STDIN_FILENO 0  /* Standard input file descriptor */
#define STDOUT_FILENO 1 /* Standard output file descriptor */
#define STDERR_FILENO 2 /* Standard error file descriptor */

/**
 * @brief  To cause the calling thread to sleep either until the number of
           real-time seconds specified in seconds have elapsed or until a
           signal arrives which is not ignored.
 * @param  seconds: The sleep seconds to provide.
 * @retval int: 0 on success and nonzero error number on error.
 */
unsigned int sleep(unsigned int seconds);

/**
 * @brief  Suspend execution of the calling thread for a given time in
 *         microseconds
 * @param  usec: The sleep microseconds to provide.
 * @retval int: 0 on success and nonzero error number on error.
 */
int usleep(useconds_t usec);

/**
 * @brief  Close a file descriptor, so that it no longer refers to any file
 *         and may be reused
 * @param  fd: The file descriptor to provide.
 * @retval int: 0 on success and nonzero error number on error.
 */
int close(int fd);

/**
 * @brief  Create a copy of the file descriptor oldfd, using the lowest-numbered
 *         unused file descriptor for the new descriptor
 * @param  oldfd: The file descriptor to duplicate.
 * @retval int: New file descriptor on success and nonzero error number on
 * error.
 */
int dup(int oldfd);

/**
 * @brief  Perform the same task as dup(), but use the file descriptor number
 *         specified by newfd.
 * @param  oldfd: The file descriptor to duplicate.
 * @param  newfd: The number to specify the new file descriptor.
 * @retval int: New file descriptor on success and nonzero error number on
 * error.
 */
int dup2(int oldfd, int newfd);

/**
 * @brief  Attempt to read up to count bytes from file descriptor fd into
           the buffer starting at buf
 * @param  fd: The file descriptor to provide.
 * @param  buf: The memory space for storing read data.
 * @param  count: The number of bytes to read.
 * @retval int: 0 on success and nonzero error number on error.
 */
ssize_t read(int fd, void *buf, size_t count);

/**
 * @brief  Write up to count bytes from the buffer starting at buf to the file
 *         referred to by the file descriptor fd
 * @param  fd: The file descriptor to provide.
 * @param  buf: The data to write.
 * @param  count: The number of bytes to write.
 * @retval int: 0 on success and nonzero error number on error.
 */
ssize_t write(int fd, const void *buf, size_t count);

/**
 * @brief  Reposition the file offset of the open file description associated
           with the file descriptor fd
 * @param  fd: The file descriptor to provide.
 * @param  offset: The new offset to the position specified by whence.
 * @param  whence: The start position of the new offset.
 * @retval int: 0 on success and nonzero error number on error.
 */
off_t lseek(int fd, long offset, int whence);

/**
 * @brief  Return the ID of the calling task
 * @param  None
 * @retval int: Task ID.
 */
int getpid(void);

/**
 * @brief  Get current working directory
 * @param  buf: Buffer space for storing path of the current working space.
 * @param  size: Size of the buffer space.
 * @retval char *: The function returns a pointer to a string containing the
 *         pathname of current working directory if success; otherwise it
 *         returns NULL.
 */
char *getcwd(char *buf, size_t size);

/**
 * @brief  Change working directory
 * @param  path: Pathname of the new working directory.
 * @retval int: 0 on success and nonzero error number on error.
 */
int chdir(const char *path);

#endif
