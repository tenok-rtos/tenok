#ifndef __POLL_H__
#define __POLL_H__

#include <stdint.h>

#define POLLIN   1
#define POLLOUT  4
#define POLLNVAL 32

typedef uint32_t nfds_t;

struct pollfd {
    int   fd;      /* file descriptor */
    short events;  /* requested events */
    short revents; /* returned events */
};

int poll(struct pollfd *fds, nfds_t nfds, int timeout);

#endif
