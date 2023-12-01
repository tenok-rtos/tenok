/**
 * @file
 */
#ifndef __POLL_H__
#define __POLL_H__

#include <stdint.h>

#define POLLIN 1
#define POLLOUT 4
#define POLLNVAL 32

typedef uint32_t nfds_t;

struct pollfd {
    int fd;        /* File descriptor number */
    short events;  /* Requested events */
    short revents; /* Returned events */
};

/**
 * @brief  Wait for one of a set of file descriptors to become ready to perform
 *         I/O
 * @param  fds: A set of file descriptors with events to wait.
 * @param  nfds: Number of the provided fds.
 * @param  timeout: The number of milliseconds that poll() should block waiting
 *         for a file descriptor to become ready. Negative value means an
 *         infinite timeout and zero causes poll() to return immediately.
 * @retval int: 0 on success and nonzero error number on error.
 */
int poll(struct pollfd *fds, nfds_t nfds, int timeout);

#endif
