/* Tenok's unistd.h shadows newlib's, which is where the getopt family lives.
 * The wrappers in user/busybox/bb_compat.c account for what an applet opens,
 * the calls that need no accounting reach Tenok directly.
 */
#ifndef _TENOK_BUSYBOX_UNISTD_H
#define _TENOK_BUSYBOX_UNISTD_H

#include_next <unistd.h>

#include <applet.h>
#include <getopt.h>

#define read(f, b, n) applet_read(f, b, n)
#define close(f) applet_close(f)
#define dup(f) applet_dup(f)
#define pipe(f) applet_pipe(f)
#define dup2(o, n) applet_dup2(o, n)
#define getcwd(b, s) applet_getcwd(b, s)

#endif
