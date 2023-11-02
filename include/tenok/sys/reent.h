#ifndef __REENT_H__
#define __REENT_H__

#include <pthread.h>

typedef struct {
    pthread_mutex_t lock;
    int fd;
} __FILE;

#endif
