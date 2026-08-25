#include <errno.h>
#include <sys/resource.h>
#include <sys/syslimits.h>
#include <tenok.h>

#include "kconfig.h"

/* Every limit of Tenok is decided when it is built */
int getrlimit(int resource, struct rlimit *rlim)
{
    if (!rlim) {
        errno = EFAULT;
        return -1;
    }

    rlim_t limit = RLIM_INFINITY;

    switch (resource) {
    case RLIMIT_NOFILE:
        limit = OPEN_MAX;
        break;
    case RLIMIT_NPROC:
        limit = THREAD_MAX;
        break;
    case RLIMIT_DATA:
    case RLIMIT_AS:
    case RLIMIT_RSS: {
        int total = minfo(HEAP_TOTAL_SIZE);

        if (total > 0)
            limit = total;

        break;
    }
    default:
        /* Tenok counts nothing else through this path */
        break;
    }

    rlim->rlim_cur = limit;
    rlim->rlim_max = limit;

    return 0;
}

int setrlimit(int resource, const struct rlimit *rlim)
{
    struct rlimit current;

    if (!rlim) {
        errno = EFAULT;
        return -1;
    }

    if (getrlimit(resource, &current) != 0)
        return -1;

    /* Asking for no more than what is in effect is a request already met */
    if (rlim->rlim_cur <= current.rlim_cur &&
        rlim->rlim_max <= current.rlim_max)
        return 0;

    errno = EPERM;

    return -1;
}
