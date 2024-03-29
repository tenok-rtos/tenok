#include <dirent.h>
#include <poll.h>
#include <stddef.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <arch/port.h>
#include <kernel/syscall.h>

NACKED int mount(const char *source, const char *target)
{
    SYSCALL(MOUNT);
}

NACKED int open(const char *pathname, int flags)
{
    SYSCALL(OPEN);
}

NACKED int _close(int fd)
{
    SYSCALL(CLOSE);
}

int close(int fd)
{
    return _close(fd);
}

NACKED int dup(int oldfd)
{
    SYSCALL(DUP);
}

NACKED int dup2(int oldfd, int newfd)
{
    SYSCALL(DUP2);
}

NACKED ssize_t _read(int fd, void *buf, size_t count)
{
    SYSCALL(READ);
}

ssize_t read(int fd, void *buf, size_t count)
{
    return _read(fd, buf, count);
}

NACKED ssize_t _write(int fd, const void *buf, size_t count)
{
    SYSCALL(WRITE);
}

ssize_t write(int fd, const void *buf, size_t count)
{
    return _write(fd, buf, count);
}

NACKED int ioctl(int fd, unsigned int cmd, unsigned long arg)
{
    SYSCALL(IOCTL);
}

NACKED off_t _lseek(int fd, long offset, int whence)
{
    SYSCALL(LSEEK);
}

long lseek(int fd, long offset, int whence)
{
    return _lseek(fd, offset, whence);
}

NACKED int _fstat(int fd, struct stat *statbuf)
{
    SYSCALL(FSTAT);
}

int fstat(int fd, struct stat *statbuf)
{
    return _fstat(fd, statbuf);
}

NACKED int opendir(const char *name, DIR *dir)
{
    SYSCALL(OPENDIR);
}

NACKED int readdir(DIR *dirp, struct dirent *dirent)
{
    SYSCALL(READDIR);
}

NACKED char *getcwd(char *buf, size_t len)
{
    SYSCALL(GETCWD);
}

NACKED int chdir(const char *path)
{
    SYSCALL(CHDIR);
}

NACKED int mknod(const char *pathname, mode_t mode, dev_t dev)
{
    SYSCALL(MKNOD);
}

NACKED int mkfifo(const char *pathname, mode_t mode)
{
    SYSCALL(MKFIFO);
}

NACKED int poll(struct pollfd *fds, nfds_t nfds, int timeout)
{
    SYSCALL(POLL);
}

/* Not implemented. The function is defined only
 * to supress the newlib warning.
 */
int _isatty(int fd)
{
    return 0;
}
