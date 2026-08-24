#include <dirent.h>
#include <poll.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

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

static NACKED int __opendir(const char *name, DIR *dir)
{
    SYSCALL(OPENDIR);
}

/* POSIX hands the caller a stream instead of taking storage from it */
DIR *opendir(const char *name)
{
    DIR *dirp = malloc(sizeof(DIR));

    if (!dirp)
        return NULL;

    if (__opendir(name, dirp) < 0) {
        free(dirp);
        return NULL;
    }

    return dirp;
}

static NACKED int __readdir(DIR *dirp, struct dirent *dirent)
{
    SYSCALL(READDIR);
}

/* The end of the directory and a failure are both a null pointer */
struct dirent *readdir(DIR *dirp)
{
    if (__readdir(dirp, &dirp->entry) != 0)
        return NULL;

    return &dirp->entry;
}

int closedir(DIR *dirp)
{
    free(dirp);

    return 0;
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

NACKED int mkdir(const char *pathname, mode_t mode)
{
    SYSCALL(MKDIR);
}

NACKED int rmdir(const char *pathname)
{
    SYSCALL(RMDIR);
}

NACKED int _unlink(const char *pathname)
{
    SYSCALL(UNLINK);
}

int unlink(const char *pathname)
{
    return _unlink(pathname);
}

NACKED int _stat(const char *pathname, struct stat *statbuf)
{
    SYSCALL(STAT);
}

int stat(const char *pathname, struct stat *statbuf)
{
    return _stat(pathname, statbuf);
}

NACKED int _rename(const char *oldpath, const char *newpath)
{
    SYSCALL(RENAME);
}

int rename(const char *oldpath, const char *newpath)
{
    return _rename(oldpath, newpath);
}

int remove(const char *pathname)
{
    struct stat statbuf;

    int retval = stat(pathname, &statbuf);
    if (retval != 0)
        return retval;

    return S_ISDIR(statbuf.st_mode) ? rmdir(pathname) : unlink(pathname);
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
