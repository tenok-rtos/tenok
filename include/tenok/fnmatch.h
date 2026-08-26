/**
 * @file
 *
 * Newlib does not ship fnmatch(), see kernel/fnmatch.c
 */
#ifndef _TENOK_FNMATCH_H
#define _TENOK_FNMATCH_H

#define FNM_NOMATCH 1

/* The values POSIX systems give them */
#define FNM_PATHNAME 0x0001 /* A slash is matched by a slash alone */
#define FNM_NOESCAPE 0x0002 /* A backslash stands for itself */
#define FNM_PERIOD 0x0004   /* A leading period is matched by a period */
#define FNM_CASEFOLD 0x0010 /* Upper and lower case are the same letter */

int fnmatch(const char *pattern, const char *string, int flags);

#endif
