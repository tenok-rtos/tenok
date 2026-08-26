/* Tenok's stdlib.h omits a few standard definitions that BusyBox relies on */
#ifndef _TENOK_BUSYBOX_STDLIB_H
#define _TENOK_BUSYBOX_STDLIB_H

#include_next <stdlib.h>

#ifndef RAND_MAX
#endif

/* BusyBox leaves its applets through exit(). On Tenok that would terminate
 * the shell thread, so it is redirected into a longjmp back to the caller.
 */
void applet_exit(int status);
#define exit(s) applet_exit(s)
#define _exit(s) applet_exit(s)

char *getenv(const char *name);
int setenv(const char *name, const char *value, int overwrite);
int unsetenv(const char *name);
int putenv(char *string);
void abort(void);
int atexit(void (*function)(void));
int system(const char *command);
int rand(void);
void srand(unsigned int seed);
/* The allocators are declared in applet.h, both sides of the layer need
 * them. See user/busybox/bb_compat.c.
 */
#include "applet.h"

#define malloc(n) applet_malloc(n)
#define free(p) applet_free(p)
#define calloc(n, s) applet_calloc(n, s)
#define realloc(p, n) applet_realloc(p, n)

#endif
