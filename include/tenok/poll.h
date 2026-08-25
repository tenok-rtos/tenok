/**
 * @file
 */
#ifndef __POLL_H__
#define __POLL_H__

#include <signal.h>
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
 * @retval int: The number of the ready descriptors on success and -1 on
 *         error, with the reason left in errno.
 */
int poll(struct pollfd *fds, nfds_t nfds, int timeout);

/**
 * @brief  Wait for one of the descriptors to become ready, with the timeout
 *         given as a timespec. Tenok blocks no signal, so the mask is
 *         accepted and has nothing to do
 * @param  fds: The descriptors to wait on.
 * @param  nfds: How many of them there are.
 * @param  timeout_ts: How long to wait, or a null pointer to wait forever.
 * @param  sigmask: The signals to block while waiting. Not used.
 * @retval int: The number of the ready descriptors on success and -1 on
 *         error, with the reason left in errno.
 */
int ppoll(struct pollfd *fds,
          nfds_t nfds,
          const struct timespec *timeout_ts,
          const sigset_t *sigmask);

#endif
