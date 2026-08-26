/**
 * The descriptors and streams an applet leaves open.
 *
 * A file an applet leaves open cannot be removed afterwards: Tenok has no
 * reference count to defer the release with, so its remove() refuses. BusyBox
 * asks a NOFORK applet to close what it opened and most do, but not all, so
 * every descriptor handed out is counted here and what is left over is closed
 * when the applet returns.
 *
 * The count is also what lets a shell close a descriptor it duplicated: a
 * Tenok descriptor is closed once nothing refers to it any more.
 */

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/termios.h>
#include <sys/time.h>
#include <unistd.h>

#include <tenok.h>

#include "../../include/busybox/applet.h"
#include "internal.h"

#include "kconfig.h"

#define FD_COUNT (THREAD_MAX + 3 + FILE_MAX) /* Tenok's file table */

static unsigned char fd_ref[FD_COUNT];

/* What each descriptor was worth when the applet started */
static unsigned char fd_saved[FD_COUNT];

/* Tenok's system calls report a failure the way POSIX does, so only the
 * errors the layer raises on its own have to be reported from here.
 */
static int fail(int err)
{
    errno = err;
    return -1;
}

void descriptor_init(void)
{
    memset(fd_ref, 0, sizeof(fd_ref));

    /* The console, referred to by the three standard descriptors */
    for (int i = 0; i < STD_STREAM_CNT; i++)
        fd_ref[i] = 1;
}

static void fd_grab(int fd)
{
    if (fd >= 0 && fd < FD_COUNT && fd_ref[fd] < 0xff)
        fd_ref[fd]++;
}

/* Gives up one reference and closes the descriptor with the last one */
static int fd_drop(int fd)
{
    if (fd < 0 || fd >= FD_COUNT)
        return 0;

    if (fd_ref[fd] == 0)
        return 0;

    if (--fd_ref[fd] > 0)
        return 0;

    return close(fd);
}

/* Tenok does the duplicating, these only keep the count that decides when a
 * descriptor an applet leaves behind is closed
 */
int applet_dup(int oldfd)
{
    int fd = dup(oldfd);

    if (fd >= 0)
        fd_grab(fd);

    return fd;
}

int applet_dup2(int oldfd, int newfd)
{
    int fd = dup2(oldfd, newfd);

    if (fd >= 0)
        fd_grab(fd);

    return fd;
}

int applet_pipe(int pipefd[2])
{
    int retval = pipe(pipefd);

    if (retval == 0) {
        fd_grab(pipefd[0]);
        fd_grab(pipefd[1]);
    }

    return retval;
}

int applet_open(const char *pathname, int flags, ...)
{
    va_list ap;

    va_start(ap, flags);
    mode_t mode = (flags & O_CREAT) ? va_arg(ap, mode_t) : 0;
    va_end(ap);

    int fd = open(pathname, flags, mode);

    if (fd < 0)
        return -1;

    fd_grab(fd);

    return fd;
}

int applet_close(int fd)
{
    /* The descriptor closes with its last user, so that an applet closing one
     * it inherited does not take it from the shell
     */
    return fd_drop(fd);
}

/* What the interrupt character check below reads. Tenok delivers no signal,
 * so the layer has to notice the character itself
 */
static struct termios tty = {
    .c_lflag = ISIG,
    .c_cc = {[VINTR] = 0x03},
};

int applet_tcgetattr(int fd, struct termios *termios_p)
{
    int retval = tcgetattr(fd, termios_p);

    if (retval == 0)
        tty = *termios_p;

    return retval;
}

int applet_tcsetattr(int fd, int actions, const struct termios *termios_p)
{
    int retval = tcsetattr(fd, actions, termios_p);

    if (retval == 0)
        tty = *termios_p;

    return retval;
}

int applet_read(int fd, void *buf, unsigned int count)
{
    /* The line discipline of the console driver does the input processing */
    int retval = read(fd, buf, count);

    if (retval <= 0)
        return retval;

    /* Tenok delivers no signal, so the interrupt character unwinds the applet
     * the way a fatal error of BusyBox does, which is what die_func is for.
     * At the prompt the line editor has cleared ISIG and deals with it itself
     */
    char *p = buf;

    if ((tty.c_lflag & ISIG) && p[0] == (char) tty.c_cc[VINTR]) {
        write(STDOUT_FILENO, "^C\n", 3);

        if (die_func) {
            xfunc_error_retval = 130; /* As a shell reports SIGINT */
            xfunc_die();
        }

        /* Nothing to unwind to, report the end of the input instead */
        return 0;
    }

    return retval;
}

/* Tenok allocates when asked, but from its own heap, which the sweep at the
 * end of a run does not reach
 */
char *applet_getcwd(char *buf, unsigned int size)
{
    if (buf)
        return getcwd(buf, size);

    if (size == 0)
        size = PATH_MAX;

    buf = applet_malloc(size);
    if (!buf)
        return NULL;

    if (!getcwd(buf, size)) {
        applet_free(buf);
        return NULL;
    }

    return buf;
}

/* Tenok's fopen() reaches open() directly, so the descriptor it hands out is
 * invisible to the table above and would escape the sweep. These two put it
 * on the books.
 */
/* fopen() of Tenok reaches open() directly, so the descriptor it hands out is
 * invisible to the table above and would escape the sweep at the end of an
 * applet. These two put it on the books.
 */
FILE *applet_fopen(const char *pathname, const char *mode)
{
    FILE *stream = fopen(pathname, mode);

    if (stream)
        fd_grab(fileno(stream));

    return stream;
}

int applet_fclose(FILE *stream)
{
    if (!stream)
        return 0;

    int fd = fileno(stream);
    int retval = fclose(stream);

    /* fclose() closed the descriptor itself, only the count is given up */
    if (fd >= 0 && fd < FD_COUNT && fd_ref[fd] > 0)
        fd_ref[fd]--;

    return retval;
}

/* POSIX asks for two times and a nanosecond each. Tenok keeps the one a
 * program asks about, so the access half is accepted and dropped
 */
int applet_utimensat(int dirfd,
                     const char *pathname,
                     const void *times,
                     int flags)
{
    const struct timespec *t = times;

    /* A null argument means now, and so does UTIME_NOW */
    if (!t || t[1].tv_nsec == UTIME_NOW)
        return utime(pathname, UTIME_TO_NOW);

    /* The caller asked for the time to be left alone */
    if (t[1].tv_nsec == UTIME_OMIT) {
        struct stat st;

        return stat(pathname, &st);
    }

    return utime(pathname, (uint32_t) t[1].tv_sec);
}

int utimes(const char *pathname, const struct timeval times[2])
{
    const struct timeval *t = times;

    if (!t)
        return utime(pathname, UTIME_TO_NOW);

    return utime(pathname, (uint32_t) t[1].tv_sec);
}

void descriptor_enter(unsigned depth)
{
    if (depth == 1)
        memcpy(fd_saved, fd_ref, sizeof(fd_saved));
}

/* Only the outermost return sweeps. Nesting one applet inside another, which
 * xargs does, leaves the inner one to the outer sweep.
 */
void descriptor_leave(unsigned depth)
{
    if (depth != 1)
        return;

    for (int fd = 3; fd < FD_COUNT; fd++) {
        while (fd_ref[fd] > fd_saved[fd])
            fd_drop(fd);
    }
}
