#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/termios.h>
#include <sys/types.h>
#include <unistd.h>

#include <arch/port.h>
#include <kernel/errno.h>
#include <kernel/syscall.h>

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

static NACKED int __fcntl(int fd, int cmd, unsigned long arg)
{
    SYSCALL(FCNTL);
}

int fcntl(int fd, int cmd, ...)
{
    va_list ap;

    /* Every command Tenok answers takes one argument, and the ones that take
     * none do not mind being handed one
     */
    va_start(ap, cmd);
    unsigned long arg = va_arg(ap, unsigned long);
    va_end(ap);

    return set_errno(__fcntl(fd, cmd, arg));
}

static NACKED int __dup2(int oldfd, int newfd)
{
    SYSCALL(DUP2);
}

int dup2(int oldfd, int newfd)
{
    return set_errno(__dup2(oldfd, newfd));
}

/* A system call of Tenok carries four arguments, and mmap() of POSIX has six.
 * Three of them are all Tenok can act on: it has no address space to honour a
 * requested address in, nothing to enforce a protection with, and nothing for
 * a mapping to be private from
 */
static NACKED void *__mmap(int fd, size_t length, off_t offset)
{
    SYSCALL(MMAP);
}

/* Every other call says it failed by answering minus one and leaving the
 * number in errno. This one answers with the address itself, so a failure has
 * to be told apart from an address by its value: the error numbers are small
 * and an address is not. The frame buffer of this board sits at 0xd0000000,
 * which read as a signed number is far below the smallest of them.
 */
#define MMAP_ERROR_MAX 256

void *mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset)
{
    void *mem = __mmap(fd, length, offset);
    long answer = (long) mem;

    if (answer < 0 && answer > -MMAP_ERROR_MAX) {
        errno = -answer;
        return MAP_FAILED;
    }

    return mem;
}

int munmap(void *addr, size_t length)
{
    /* Nothing was mapped, so there is nothing to give back. Tenok keeps no
     * list of what was handed out, so the only thing that can be said about
     * an address is whether it could have come from mmap() at all.
     */
    if (!addr || addr == MAP_FAILED || !length) {
        errno = EINVAL;
        return -1;
    }

    return 0;
}

static NACKED int __pipe(int pipefd[2])
{
    SYSCALL(PIPE);
}

int pipe(int pipefd[2])
{
    return set_errno(__pipe(pipefd));
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

static NACKED int __ioctl(int fd, unsigned long request, unsigned long arg)
{
    SYSCALL(IOCTL);
}

int ioctl(int fd, unsigned long request, ...)
{
    va_list ap;

    va_start(ap, request);
    unsigned long arg = va_arg(ap, unsigned long);
    va_end(ap);

    return set_errno(__ioctl(fd, request, arg));
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
static NACKED char *__getcwd(char *buf, size_t len)
{
    SYSCALL(GETCWD);
}

/* A null buffer asks the library to allocate one, which is what glibc does
 * and what a caller that does not want to guess the length relies on
 */
char *getcwd(char *buf, size_t size)
{
    if (buf)
        return __getcwd(buf, size);

    if (size == 0)
        size = PATH_MAX;

    buf = malloc(size);
    if (!buf) {
        errno = ENOMEM;
        return NULL;
    }

    if (!__getcwd(buf, size)) {
        free(buf);
        errno = ERANGE;
        return NULL;
    }

    return buf;
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

/* umask() has no way to fail, there is no error number to translate */
NACKED mode_t umask(mode_t mask)
{
    SYSCALL(UMASK);
}

static NACKED int __utime(const char *pathname, uint32_t mtime)
{
    SYSCALL(UTIME);
}

/* POSIX spells this with two times and a timespec each, Tenok keeps one */
int utime(const char *pathname, uint32_t mtime)
{
    return set_errno(__utime(pathname, mtime));
}

static NACKED int __chmod(const char *pathname, mode_t mode)
{
    SYSCALL(CHMOD);
}

int chmod(const char *pathname, mode_t mode)
{
    return set_errno(__chmod(pathname, mode));
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

/* Tenok gives a file one name, so nothing a path names is a link to stop at */
int lstat(const char *pathname, struct stat *statbuf)
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

/* Tenok has a single user and checks no permission bit, so a file that
 * exists can be reached every way the caller asks about
 */
int access(const char *pathname, int mode)
{
    struct stat statbuf;

    return stat(pathname, &statbuf);
}

static NACKED int __poll(struct pollfd *fds, nfds_t nfds, int timeout)
{
    SYSCALL(POLL);
}

int poll(struct pollfd *fds, nfds_t nfds, int timeout)
{
    return set_errno(__poll(fds, nfds, timeout));
}

int ppoll(struct pollfd *fds,
          nfds_t nfds,
          const struct timespec *timeout_ts,
          const sigset_t *sigmask)
{
    int timeout = -1;

    if (timeout_ts)
        timeout = (timeout_ts->tv_sec * 1000) + (timeout_ts->tv_nsec / 1000000);

    return poll(fds, nfds, timeout);
}

/* A terminal is a device that answers the settings of a line discipline,
 * which is what POSIX means by one
 */
int isatty(int fd)
{
    struct termios termios;

    if (ioctl(fd, TCGETS, (unsigned long) &termios) != 0) {
        errno = ENOTTY;
        return 0;
    }

    return 1;
}

int _isatty(int fd)
{
    return isatty(fd);
}
