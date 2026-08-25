#include <errno.h>
#include <string.h>
#include <sys/utsname.h>

/* Every name of the system comes from the build, the kernel is not asked */
int uname(struct utsname *buf)
{
    if (!buf) {
        errno = EFAULT;
        return -1;
    }

    memset(buf, 0, sizeof(*buf));

    strncpy(buf->sysname, "Tenok", _UTSNAME_LENGTH - 1);
    strncpy(buf->nodename, __BOARD_NAME__, _UTSNAME_LENGTH - 1);
    strncpy(buf->release, __REVISION__, _UTSNAME_LENGTH - 1);
    strncpy(buf->version, __TIMESTAMP__, _UTSNAME_LENGTH - 1);
    strncpy(buf->machine, __ARCH__, _UTSNAME_LENGTH - 1);

    return 0;
}
