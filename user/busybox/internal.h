/**
 * What the three files of this directory share.
 *
 * BusyBox is written for a system where an applet runs as a process and
 * leaving it hands the memory, the descriptors and the globals back to the
 * kernel. Tenok reaches an applet by calling it, so what a process exit does
 * elsewhere is done here: applet.c runs the applet, memory.c takes back what
 * it allocated and descriptor.c what it left open.
 */
#ifndef _TENOK_BUSYBOX_INTERNAL_H
#define _TENOK_BUSYBOX_INTERNAL_H

/* Tenok's errno.h only carries the numbers, the variable itself comes from
 * newlib and its header is shadowed
 */
extern int *__errno(void);
#define errno (*__errno())

/* BusyBox unwinds a failing applet through these. FAST_FUNC is empty on ARM,
 * so the declarations of libbb.h need not be visible here.
 */
extern void (*die_func)(void);
extern int xfunc_error_retval;
void xfunc_die(void);

/* Each side is told when an applet starts and when it returns, and takes back
 * what that applet left behind. The depth says how deep the nesting is, which
 * xargs makes possible: only the outermost return sweeps.
 */
void memory_enter(unsigned depth);
void memory_leave(unsigned depth);
void memory_release_all(void);

void descriptor_init(void);
void descriptor_enter(unsigned depth);
void descriptor_leave(unsigned depth);

#endif
