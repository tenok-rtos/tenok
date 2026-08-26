/**
 * Shared between the BusyBox side of the compatibility layer and its
 * implementation in user/busybox/applet.c.
 *
 * The implementation is compiled against Tenok's own headers, so it cannot
 * include <sys/stat.h> or <dirent.h> from this directory. Everything both
 * sides have to agree on lives here and is included by explicit path.
 */
#ifndef _TENOK_BUSYBOX_APPLET_H
#define _TENOK_BUSYBOX_APPLET_H

#include <stddef.h>
#include <stdint.h>
#include <sys/limits.h>

/* Tenok's own <sys/stat.h> gives the POSIX layout, so nothing is translated
 * here any more: the wrappers below take Tenok's struct stat as it is.
 */
struct stat;

int applet_utimensat(int dirfd,
                     const char *pathname,
                     const void *times,
                     int flags);

typedef struct dirstream DIR;


int applet_dup(int oldfd);
int applet_pipe(int pipefd[2]);
int applet_dup2(int oldfd, int newfd);

int applet_open(const char *pathname, int flags, ...);
int applet_read(int fd, void *buf, unsigned int count);
int applet_close(int fd);
char *applet_getcwd(char *buf, unsigned int size);

/* Every BusyBox allocation goes through Tenok's heap with a size header, so
 * that realloc() can work
 */
void *applet_malloc(size_t size);
void applet_free(void *ptr);
void *applet_calloc(size_t nmemb, size_t size);
void *applet_realloc(void *ptr, size_t size);

/* The settings live in the driver of the terminal. These two carry them
 * across, translating the descriptor on the way
 */
struct termios;

int applet_tcgetattr(int fd, struct termios *termios_p);
int applet_tcsetattr(int fd, int actions, const struct termios *termios_p);


#endif
