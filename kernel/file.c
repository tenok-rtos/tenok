#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <arch/port.h>
#include <kernel/syscall.h>

/* The kernel answers a failure with the negation of its error number, POSIX
 * asks for minus one with the number left in errno
 */
static long set_errno(long retval)
{
    if (retval < 0) {
        errno = -retval;
        return -1;
    }

    return retval;
}

/* The __ functions are the raw traps, the ones named after the call
 * translate what comes back
 */
static NACKED int __mount(const char *source, const char *target)
{
    SYSCALL(MOUNT);
}

int mount(const char *source, const char *target)
{
    return set_errno(__mount(source, target));
}

static NACKED int __open(const char *pathname, int flags, mode_t mode)
{
    SYSCALL(OPEN);
}

int open(const char *pathname, int flags, ...)
{
    va_list ap;

    /* The mode is only given when the file may have to be created */
    va_start(ap, flags);
    mode_t mode = (flags & O_CREAT) ? va_arg(ap, mode_t) : 0;
    va_end(ap);

    return set_errno(__open(pathname, flags, mode));
}

static NACKED int __close(int fd)
{
    SYSCALL(CLOSE);
}

int close(int fd)
{
    return set_errno(__close(fd));
}

int _close(int fd)
{
    return close(fd);
}

static NACKED int __dup(int oldfd)
{
    SYSCALL(DUP);
}

int dup(int oldfd)
{
    return set_errno(__dup(oldfd));
}

static NACKED int __dup2(int oldfd, int newfd)
{
    SYSCALL(DUP2);
}

int dup2(int oldfd, int newfd)
{
    return set_errno(__dup2(oldfd, newfd));
}

static NACKED ssize_t __read(int fd, void *buf, size_t count)
{
    SYSCALL(READ);
}

ssize_t read(int fd, void *buf, size_t count)
{
    return set_errno(__read(fd, buf, count));
}

ssize_t _read(int fd, void *buf, size_t count)
{
    return read(fd, buf, count);
}

static NACKED ssize_t __write(int fd, const void *buf, size_t count)
{
    SYSCALL(WRITE);
}

ssize_t write(int fd, const void *buf, size_t count)
{
    return set_errno(__write(fd, buf, count));
}

ssize_t _write(int fd, const void *buf, size_t count)
{
    return write(fd, buf, count);
}

static NACKED int __ioctl(int fd, unsigned int cmd, unsigned long arg)
{
    SYSCALL(IOCTL);
}

int ioctl(int fd, unsigned int cmd, unsigned long arg)
{
    return set_errno(__ioctl(fd, cmd, arg));
}

static NACKED off_t __lseek(int fd, long offset, int whence)
{
    SYSCALL(LSEEK);
}

long lseek(int fd, long offset, int whence)
{
    return set_errno(__lseek(fd, offset, whence));
}

off_t _lseek(int fd, long offset, int whence)
{
    return lseek(fd, offset, whence);
}

static NACKED int __fstat(int fd, struct stat *statbuf)
{
    SYSCALL(FSTAT);
}

int fstat(int fd, struct stat *statbuf)
{
    return set_errno(__fstat(fd, statbuf));
}

int _fstat(int fd, struct stat *statbuf)
{
    return fstat(fd, statbuf);
}

static NACKED int __opendir(const char *name, DIR *dir)
{
    SYSCALL(OPENDIR);
}

/* POSIX hands the caller a stream instead of taking storage from it */
DIR *opendir(const char *name)
{
    DIR *dirp = malloc(sizeof(DIR));

    if (!dirp) {
        errno = ENOMEM;
        return NULL;
    }

    if (set_errno(__opendir(name, dirp)) < 0) {
        free(dirp);
        return NULL;
    }

    return dirp;
}

static NACKED int __readdir(DIR *dirp, struct dirent *dirent)
{
    SYSCALL(READDIR);
}

/* The end of the directory and a failure are told apart by errno */
struct dirent *readdir(DIR *dirp)
{
    if (__readdir(dirp, &dirp->entry) != 0)
        return NULL;

    return &dirp->entry;
}

int closedir(DIR *dirp)
{
    if (!dirp) {
        errno = EBADF;
        return -1;
    }

    free(dirp);

    return 0;
}

/* getcwd() answers with a pointer, there is no error number to translate */
NACKED char *getcwd(char *buf, size_t len)
{
    SYSCALL(GETCWD);
}

static NACKED int __chdir(const char *path)
{
    SYSCALL(CHDIR);
}

int chdir(const char *path)
{
    return set_errno(__chdir(path));
}

static NACKED int __mknod(const char *pathname, mode_t mode, dev_t dev)
{
    SYSCALL(MKNOD);
}

int mknod(const char *pathname, mode_t mode, dev_t dev)
{
    return set_errno(__mknod(pathname, mode, dev));
}

static NACKED int __mkfifo(const char *pathname, mode_t mode)
{
    SYSCALL(MKFIFO);
}

int mkfifo(const char *pathname, mode_t mode)
{
    return set_errno(__mkfifo(pathname, mode));
}

static NACKED int __mkdir(const char *pathname, mode_t mode)
{
    SYSCALL(MKDIR);
}

int mkdir(const char *pathname, mode_t mode)
{
    return set_errno(__mkdir(pathname, mode));
}

static NACKED int __rmdir(const char *pathname)
{
    SYSCALL(RMDIR);
}

int rmdir(const char *pathname)
{
    return set_errno(__rmdir(pathname));
}

static NACKED int __unlink(const char *pathname)
{
    SYSCALL(UNLINK);
}

int unlink(const char *pathname)
{
    return set_errno(__unlink(pathname));
}

int _unlink(const char *pathname)
{
    return unlink(pathname);
}

static NACKED int __stat(const char *pathname, struct stat *statbuf)
{
    SYSCALL(STAT);
}

int stat(const char *pathname, struct stat *statbuf)
{
    return set_errno(__stat(pathname, statbuf));
}

int _stat(const char *pathname, struct stat *statbuf)
{
    return stat(pathname, statbuf);
}

static NACKED int __rename(const char *oldpath, const char *newpath)
{
    SYSCALL(RENAME);
}

int rename(const char *oldpath, const char *newpath)
{
    return set_errno(__rename(oldpath, newpath));
}

int _rename(const char *oldpath, const char *newpath)
{
    return rename(oldpath, newpath);
}

int remove(const char *pathname)
{
    struct stat statbuf;

    int retval = stat(pathname, &statbuf);
    if (retval != 0)
        return retval;

    return S_ISDIR(statbuf.st_mode) ? rmdir(pathname) : unlink(pathname);
}

static NACKED int __poll(struct pollfd *fds, nfds_t nfds, int timeout)
{
    SYSCALL(POLL);
}

int poll(struct pollfd *fds, nfds_t nfds, int timeout)
{
    return set_errno(__poll(fds, nfds, timeout));
}

/* Not implemented. The function is defined only
 * to supress the newlib warning.
 */
int _isatty(int fd)
{
    return 0;
}
