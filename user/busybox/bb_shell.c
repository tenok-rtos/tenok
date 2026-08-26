/**
 * Hook that lets the Tenok shell run the BusyBox multi-call binary.
 *
 * BusyBox is linked into the firmware rather than executed, so its main() is
 * renamed to busybox_entry() at build time and reached through applet_run(),
 * which also catches the exit() that ends every applet.
 */

#include "shell.h"

int applet_run(int argc, char *argv[]);

static int busybox(int argc, char *argv[])
{
    return applet_run(argc, argv);
}

HOOK_SHELL_CMD("busybox", busybox);
