/* Tenok's open() takes the mode as a variadic argument and its fcntl() is the
 * POSIX one. Only the bookkeeping of the descriptor goes through the layer
 */
#ifndef _TENOK_BUSYBOX_FCNTL_H
#define _TENOK_BUSYBOX_FCNTL_H

#include_next <fcntl.h>

#include <applet.h>

#define open(...) applet_open(__VA_ARGS__)

#endif
