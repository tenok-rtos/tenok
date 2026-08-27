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
 * @brief  Create a pipe, a stream that is written to through one descriptor
 *         and read from through the other.
 * @param  pipefd: Where to store the two descriptors, the read end first and
 *         the write end second.
 * @retval int: 0 on success and -1 on error, with the reason left in errno.
 */
int pipe(int pipefd[2]);

/**
 * @brief  Create a child process. Tenok runs every task from the image it
 *         booted and has no way to make another, so the call always fails.
 * @retval pid_t: -1 with errno set to ENOSYS.
 */
pid_t vfork(void);

/**
 * @brief  Create a child process, the way vfork() does not either.
 * @retval pid_t: -1 with errno set to ENOSYS.
 */
pid_t fork(void);

/**
 * @brief  Start a session. Tenok has the one console and no sessions to give
 *         it out to.
 * @retval pid_t: -1 with errno set to ENOSYS.
 */
pid_t setsid(void);

/**
 * @brief  Replace the running program with the one the pathname names. No
 *         file of Tenok is in a format the system can execute, so the call
 *         always fails.
 * @param  pathname: The pathname of the program.
 * @param  argv: The arguments to hand the program.
 * @param  envp: The environment to hand the program.
 * @retval int: -1 with errno set to ENOEXEC.
 */
int execve(const char *pathname, char *const argv[], char *const envp[]);

/**
 * @brief  Replace the running program with the one found by searching PATH
 *         for the file, the way execve() does.
 * @param  file: The name of the program to search for.
 * @param  argv: The arguments to hand the program.
 * @retval int: -1 with errno set to ENOEXEC.
 */
int execvp(const char *file, char *const argv[]);

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
 * @retval pid_t: The task ID.
 */
pid_t getpid(void);

/**
 * @brief  Read the identifier of the calling thread. Linux names this, and a
 *         program written for it asks for it where POSIX would use
 *         pthread_self()
 * @retval pid_t: The thread identifier.
 */
pid_t gettid(void);

/**
 * @brief  Return the ID of the task that started the calling one. Every task
 *         of Tenok is started by the one that brings the system up
 * @retval pid_t: Always 1.
 */
pid_t getppid(void);

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

/**
 * @brief  Become the user of the given identifier. Tenok runs as the one user
 *         it has, so only that one can be become.
 * @param  uid: The user to become.
 * @retval int: 0 for the user Tenok runs as, and -1 with errno set to EPERM
 *         for any other.
 */
int setuid(uid_t uid);

/**
 * @brief  Become the effective user of the given identifier, the way setuid()
 *         does.
 * @param  uid: The user to become.
 * @retval int: 0 for the user Tenok runs as, and -1 with errno set to EPERM
 *         for any other.
 */
int seteuid(uid_t uid);
uid_t geteuid(void);

/**
 * @brief  Report the group a task runs as, which on Tenok is always root
 * @retval gid_t: Always 0.
 */
gid_t getgid(void);

/**
 * @brief  Join the group of the given identifier. Tenok has the one group, so
 *         only that one can be joined.
 * @param  gid: The group to join.
 * @retval int: 0 for the group Tenok runs as, and -1 with errno set to EPERM
 *         for any other.
 */
int setgid(gid_t gid);

/**
 * @brief  Join the effective group of the given identifier, the way setgid()
 *         does.
 * @param  gid: The group to join.
 * @retval int: 0 for the group Tenok runs as, and -1 with errno set to EPERM
 *         for any other.
 */
int setegid(gid_t gid);
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
 * @brief  Give a file a second name. Tenok gives a file one name.
 * @param  oldpath: The file to name again.
 * @param  newpath: The name to give it.
 * @retval int: -1 with errno set to ENOSYS.
 */
int link(const char *oldpath, const char *newpath);

/**
 * @brief  Make a name that stands for another. Tenok has no symbolic link.
 * @param  target: What the name would stand for.
 * @param  linkpath: The name to make.
 * @retval int: -1 with errno set to ENOSYS.
 */
int symlink(const char *target, const char *linkpath);

/**
 * @brief  Read what a symbolic link stands for. Nothing a path of Tenok names
 *         is one.
 * @param  pathname: The name to read.
 * @param  buf: Where to put what it stands for.
 * @param  bufsiz: The room there is for it.
 * @retval int: -1 with errno set to EINVAL, which is what POSIX says of a
 *         name that is not a symbolic link.
 */
int readlink(const char *pathname, char *buf, size_t bufsiz);

/**
 * @brief  Change the working directory to the one the descriptor names. Tenok
 *         keeps no way back from a descriptor to the name it was opened by.
 * @param  fd: The descriptor of the directory.
 * @retval int: -1 with errno set to ENOSYS.
 */
int fchdir(int fd);

/**
 * @brief  Name the terminal the descriptor is open on. Tenok keeps no way
 *         back from a descriptor to the name it was opened by.
 * @param  fd: The descriptor.
 * @param  buf: Where to put the name.
 * @param  buflen: The room there is for it.
 * @retval int: ENOSYS.
 */
int ttyname_r(int fd, char *buf, size_t buflen);

/**
 * @brief  Make the given directory the root. The root of Tenok is the whole
 *         of what it mounted, with nothing outside to be shut away from.
 * @param  path: The directory to make the root.
 * @retval int: -1 with errno set to ENOSYS.
 */
int chroot(const char *path);

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

/* Tenok keeps no owner for an open file and has no way to shorten one. They
 * are named so that a program written against POSIX builds
 */

/**
 * @brief  Replace the owner of an open file, which Tenok does not keep.
 * @param  fd: The open file.
 * @param  owner: The user to give it to.
 * @param  group: The group to give it to.
 * @retval int: -1 with errno set to ENOSYS.
 */
int fchown(int fd, uid_t owner, gid_t group);

/**
 * @brief  Shorten an open file, which Tenok has no way to do.
 * @param  fd: The open file.
 * @param  length: What to shorten it to.
 * @retval int: -1 with errno set to ENOSYS.
 */
int ftruncate(int fd, off_t length);

#endif
