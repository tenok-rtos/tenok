/* Tenok has strdup() and strndup() of its own, but they take the memory from
 * Tenok's heap, which the sweep at the end of an applet does not reach.
 * BusyBox duplicates strings constantly and frees few of them, so its copies
 * have to come from the allocator the sweep owns.
 */
#ifndef _TENOK_BUSYBOX_STRING_H
#define _TENOK_BUSYBOX_STRING_H

#include_next <string.h>

char *applet_strdup(const char *s);
char *applet_strndup(const char *s, size_t n);

#define strdup(s) applet_strdup(s)
#define strndup(s, n) applet_strndup(s, n)

#endif
