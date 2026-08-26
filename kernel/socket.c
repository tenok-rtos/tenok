#include <errno.h>
#include <stddef.h>
#include <sys/socket.h>
#include <sys/types.h>

/* Tenok carries no network. The calls are here so that a program which asks
 * for one is told there is none, rather than failing to link
 */
int socket(int domain, int type, int protocol)
{
    errno = ENOSYS;
    return -1;
}

int bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen)
{
    errno = ENOSYS;
    return -1;
}

int listen(int sockfd, int backlog)
{
    errno = ENOSYS;
    return -1;
}

ssize_t sendto(int sockfd,
               const void *buf,
               size_t len,
               int flags,
               const struct sockaddr *dest_addr,
               socklen_t addrlen)
{
    errno = ENOSYS;
    return -1;
}
