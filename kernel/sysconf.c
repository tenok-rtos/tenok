#include <errno.h>
#include <sys/syslimits.h>
#include <unistd.h>

#include "kconfig.h"

/* Every value here is decided when Tenok is built */
long _sysconf(int name)
{
    switch (name) {
    case _SC_CLK_TCK:
        return OS_TICK_FREQ;
    case _SC_OPEN_MAX:
        return OPEN_MAX;
    case _SC_PAGESIZE:
        return FS_BLK_SIZE;
    default:
        errno = EINVAL;
        return -1;
    }
}
