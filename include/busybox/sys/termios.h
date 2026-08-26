/* Tenok's own <sys/termios.h> has the settings. The descriptor is one of the
 * layer's own, so the two calls go through it
 */
#ifndef _TENOK_BUSYBOX_SYS_TERMIOS_H
#define _TENOK_BUSYBOX_SYS_TERMIOS_H

#include_next <sys/termios.h>

#include "../applet.h"

#define tcgetattr(fd, t) applet_tcgetattr(fd, t)
#define tcsetattr(fd, a, t) applet_tcsetattr(fd, a, t)

#endif
