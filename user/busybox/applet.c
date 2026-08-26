/**
 * The process an applet does not have.
 *
 * BusyBox reaches an applet through a process: fork() gives it globals as the
 * linker laid them out, and exit() hands everything back. Tenok has neither,
 * so applet_run() lays the globals out again before the call and takes back
 * afterwards what the exit would have taken.
 *
 * This file is compiled against Tenok's own headers, not against
 * include/busybox, so that it can reach the real system calls.
 */

#include <setjmp.h>
#include <string.h>

#include <tenok.h>

#include "internal.h"

int busybox_entry(int argc, char **argv);

/* Boundaries of the BusyBox globals, laid out by the linker script */
extern char _bb_data_start[], _bb_data_end[], _bb_data_load[];
extern char _bb_bss_start[], _bb_bss_end[];

static jmp_buf exit_jmp;
static int exit_armed;
static int exit_status;

/* How deep the applets are nested, which xargs makes possible */
static unsigned depth;

void applet_exit(int status)
{
    /* Inside a NOFORK applet, leaving means returning to whoever called it
     * and not ending the program. BusyBox asks its applets to use
     * xfunc_die() for that, and most do, but some call exit() outright: dd
     * and uniq would otherwise take the whole shell down with them.
     */
    if (die_func) {
        xfunc_error_retval = status;
        xfunc_die();
    }

    exit_status = status;

    if (exit_armed)
        longjmp(exit_jmp, 1);

    /* Nothing to unwind to, the caller is not a BusyBox applet */
    while (1)
        ;
}

/* BusyBox is written for a system where a command runs as a process, so it
 * expects its globals to start out as the program was linked. Restoring them
 * gives an applet that, and it is what lets applet_run() release everything the
 * run allocated: no static is left holding a pointer into the freed memory.
 */
static void reset_globals(void)
{
    memcpy(_bb_data_start, _bb_data_load, _bb_data_end - _bb_data_start);
    memset(_bb_bss_start, 0, _bb_bss_end - _bb_bss_start);

    /* The descriptors of the previous run go with it */
    descriptor_init();
}

/* Entry point used by the shell command */
int applet_run(int argc, char **argv)
{
    reset_globals();

    exit_status = 0;
    exit_armed = 1;

    if (setjmp(exit_jmp) == 0)
        exit_status = busybox_entry(argc, argv);

    exit_armed = 0;

    memory_release_all();

    return exit_status;
}

/* BusyBox tells the layer when an applet starts and when it returns, through
 * the hooks the Tenok patch adds to run_nofork_applet()
 */
void applet_enter(void)
{
    depth++;
    memory_enter(depth);
    descriptor_enter(depth);
}

void applet_leave(void)
{
    memory_leave(depth);
    descriptor_leave(depth);

    if (depth > 0)
        depth--;
}
