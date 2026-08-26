/* Tenok's stdio.h plus the pieces BusyBox expects from a POSIX libc */
#ifndef _TENOK_BUSYBOX_STDIO_H
#define _TENOK_BUSYBOX_STDIO_H

#include_next <stdio.h>

#include <stdarg.h>

FILE *applet_fopen(const char *pathname, const char *mode);
int applet_fclose(FILE *stream);

/* Tenok's stdio.h points these at newlib's FILE; take them back */
#undef fopen
#undef fclose
#define fopen(p, m) applet_fopen(p, m)
#define fclose(f) applet_fclose(f)

int vasprintf(char **strp, const char *format, va_list ap);

#endif
