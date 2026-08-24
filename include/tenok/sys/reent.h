/**
 * @file
 */
#ifndef __REENT_H__
#define __REENT_H__

#include <pthread.h>

typedef struct {
    int fd;
    /* Set by the end of the file and by a failure, until clearerr() */
    int eof;
    int err;
} __FILE;

#endif
