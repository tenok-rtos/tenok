/**
 * @file
 */
#ifndef __UTSNAME_H__
#define __UTSNAME_H__

/* The length POSIX systems give these, the null byte included */
#define _UTSNAME_LENGTH 65

struct utsname {
    char sysname[_UTSNAME_LENGTH];    /* The system, always "Tenok" */
    char nodename[_UTSNAME_LENGTH];   /* The board it was built for */
    char release[_UTSNAME_LENGTH];    /* The revision it was built from */
    char version[_UTSNAME_LENGTH];    /* When it was built */
    char machine[_UTSNAME_LENGTH];    /* The architecture */
    char domainname[_UTSNAME_LENGTH]; /* Tenok has no network, always empty */
};

/**
 * @brief  Return the name and the version of the system
 * @param  buf: The buffer for returning the names.
 * @retval int: 0 on success and -1 on error, with the reason left in errno.
 */
int uname(struct utsname *buf);

#endif
