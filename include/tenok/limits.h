/**
 * @file
 */
#ifndef __LIMITS_H__
#define __LIMITS_H__

/* The limits of the machine, which the compiler is the one to know */
#include_next <limits.h>

/* The limits the system decides, which POSIX names here */
#include <sys/syslimits.h>

#endif
