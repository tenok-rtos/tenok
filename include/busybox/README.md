BusyBox compatibility headers
=============================

BusyBox assumes a Linux/glibc environment. `include/libbb.h` declares the whole
POSIX surface unconditionally, no matter which applets the configuration
enables, so even a two applet build needs the socket, user database and
terminal types to exist.

These headers exist only to satisfy that surface. They are placed ahead of
Tenok's own headers when BusyBox is compiled and are never used by Tenok
itself, so none of Tenok's public headers had to change for the port.

Anything that cannot be solved from here lives in the patch series under
`lib/package-patches/busybox/patches`.
