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
 * @retval int: 0 on success and -1 on error, with the reason left in errno.
 */
int close(int fd);

/**
 * @brief  Create a copy of the file descriptor oldfd, using the lowest-numbered
 *         unused file descriptor for the new descriptor
 * @param  oldfd: The file descriptor to duplicate.
 * @retval int: The new file descriptor on success and -1 on error, with the
 *         reason left in errno.
 */
int dup(int oldfd);

/**
 * @brief  Perform the same task as dup(), but use the file descriptor number
 *         specified by newfd.
 * @param  oldfd: The file descriptor to duplicate.
 * @param  newfd: The number to specify the new file descriptor.
 * @retval int: The new file descriptor on success and -1 on error, with the
 *         reason left in errno.
 */
int dup2(int oldfd, int newfd);

/**
 * @brief  Attempt to read up to count bytes from file descriptor fd into
           the buffer starting at buf
 * @param  fd: The file descriptor to provide.
 * @param  buf: The memory space for storing read data.
 * @param  count: The number of bytes to read.
 * @retval int: 0 on success and -1 on error, with the reason left in errno.
 */
ssize_t read(int fd, void *buf, size_t count);

/**
 * @brief  Write up to count bytes from the buffer starting at buf to the file
 *         referred to by the file descriptor fd
 * @param  fd: The file descriptor to provide.
 * @param  buf: The data to write.
 * @param  count: The number of bytes to write.
 * @retval int: 0 on success and -1 on error, with the reason left in errno.
 */
ssize_t write(int fd, const void *buf, size_t count);

/**
 * @brief  Reposition the file offset of the open file description associated
           with the file descriptor fd
 * @param  fd: The file descriptor to provide.
 * @param  offset: The new offset to the position specified by whence.
 * @param  whence: The start position of the new offset.
 * @retval off_t: The new offset on success and -1 on error, with the reason
 *         left in errno.
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
 * @param  buf: Buffer space for storing path of the current working space,
 *         or a null pointer to ask for one that free() takes back.
 * @retval char *: The buffer on success and a null pointer on error, with
 *         the reason left in errno.
 */
char *getcwd(char *buf, size_t size);

/**
 * @brief  Change working directory
 * @param  path: Pathname of the new working directory.
 * @retval int: 0 on success and -1 on error, with the reason left in errno.
 */
int chdir(const char *path);

/**
 * @brief  Delete a name from the file system
 * @param  pathname: The pathname of the file to remove.
 * @retval int: 0 on success and -1 on error, with the reason left in errno.
 */
int unlink(const char *pathname);

/**
 * @brief  Delete a directory, which must be empty
 * @param  pathname: The pathname of the directory to remove.
 * @retval int: 0 on success and -1 on error, with the reason left in errno.
 */
int rmdir(const char *pathname);

/* Values sysconf() reports, with the numbers Linux gives them */
#define _SC_ARG_MAX 0
#define _SC_CLK_TCK 2
#define _SC_OPEN_MAX 4
#define _SC_PAGESIZE 8

/**
 * @brief  Report a value of the system that is decided when it is built
 * @param  name: One of the _SC_ values.
 * @retval long: The value on success and -1 when Tenok has no such value,
 *         with the reason left in errno.
 */
/* Newlib ships one that answers for a system Tenok is not */
#define sysconf _sysconf
long sysconf(int name);

/* Accessibility asked about by access() */
#define F_OK 0 /* The file exists */
#define X_OK 1 /* The file can be executed */
#define W_OK 2 /* The file can be written */
#define R_OK 4 /* The file can be read */

/**
 * @brief  Tell whether a descriptor refers to a terminal
 * @param  fd: The file descriptor to provide.
 * @retval int: One when it does, and zero when it does not, with the reason
 *         left in errno.
 */
int isatty(int fd);

/**
 * @brief  Check whether the calling task can reach a file the way it asks
 * @param  pathname: The pathname of the file to check.
 * @param  mode: F_OK, or the R_OK, W_OK and X_OK to check for.
 * @retval int: 0 on success and -1 on error, with the reason left in errno.
 */
int access(const char *pathname, int mode);

/**
 * @brief  Report the user a task runs as, which on Tenok is always root
 * @retval uid_t: Always 0.
 */
uid_t getuid(void);
uid_t geteuid(void);

/**
 * @brief  Report the group a task runs as, which on Tenok is always root
 * @retval gid_t: Always 0.
 */
gid_t getgid(void);
gid_t getegid(void);

/**
 * @brief  Report the supplementary groups of the task. Tenok has one group
 *         and root is in it, so there is never a supplementary one
 * @param  size: How many the list holds.
 * @param  list: For returning the groups.
 * @retval int: Always 0.
 */
int getgroups(int size, gid_t list[]);

/**
 * @brief  Replace the owner of a file. Tenok has one user, so only a request
 *         that names it can be granted
 * @param  pathname: The pathname of the file.
 * @param  owner: The user to give it to, or -1 to leave it alone.
 * @param  group: The group to give it to, or -1 to leave it alone.
 * @retval int: 0 on success and -1 on error, with the reason left in errno.
 */
int chown(const char *pathname, uid_t owner, gid_t group);

/**
 * @brief  Replace the owner of a file without following a symbolic link.
 *         Tenok has no symbolic link, so this is chown()
 * @param  pathname: The pathname of the file.
 * @param  owner: The user to give it to, or -1 to leave it alone.
 * @param  group: The group to give it to, or -1 to leave it alone.
 * @retval int: 0 on success and -1 on error, with the reason left in errno.
 */
int lchown(const char *pathname, uid_t owner, gid_t group);

#endif
