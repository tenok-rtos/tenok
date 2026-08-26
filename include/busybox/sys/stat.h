/* Tenok's own <sys/stat.h> gives the POSIX file mode and the POSIX stat
 * buffer, so only the three calls Tenok does not have are answered here.
 */
#ifndef _TENOK_BUSYBOX_SYS_STAT_H
#define _TENOK_BUSYBOX_SYS_STAT_H
#include <applet.h>
#include_next <sys/stat.h>
/* fstat() takes a descriptor, and the descriptors BusyBox holds are the ones
 * of the compatibility layer rather than Tenok's. Object like on purpose, so
 * that BusyBox can also take the address of the call.
 */
/* Tenok has no symbolic links, so there is nothing for lstat() to see that
 * stat() does not.
 */
#define utimensat(d, p, t, f) applet_utimensat(d, p, t, f)
#endif
