/**
 * @file
 */
/* Newlib declares basename() in <string.h> but ships neither it nor dirname()
 */
#ifndef _TENOK_LIBGEN_H
#define _TENOK_LIBGEN_H

char *dirname(char *path);

#endif
