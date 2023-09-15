#include <fs/fs.h>
#include <arch/port.h>
#include <kernel/syscall.h>

#include <tenok/poll.h>
#include <tenok/sys/stat.h>

NACKED int mount(const char *source, const char *target)
{
    SYSCALL(MOUNT);
}

NACKED int open(const char *pathname, int flags, mode_t mode)
{
    SYSCALL(OPEN);
}

NACKED int close(int fd)
{
    SYSCALL(CLOSE);
}

NACKED ssize_t read(int fd, void *buf, size_t count)
{
    SYSCALL(READ);
}

NACKED ssize_t write(int fd, const void *buf, size_t count)
{
    SYSCALL(WRITE);
}

NACKED long lseek(int fd, long offset, int whence)
{
    SYSCALL(LSEEK);
}

NACKED int fstat(int fd, struct stat *statbuf)
{
    SYSCALL(FSTAT);
}

NACKED int opendir(const char *name, DIR *dir)
{
    SYSCALL(OPENDIR);
}

NACKED int readdir(DIR *dirp, struct dirent *dirent)
{
    SYSCALL(READDIR);
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
