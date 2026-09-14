/**
 * @file
 * Sockets.
 *
 * The system has no network of its own. What it has, when it is built with
 * one, is a stack beside it, and this is where a call made in the shape this
 * system publishes is turned into the call the stack answers to.
 *
 * Two things differ between the two shapes and are put right here. An address
 * is laid out the way the older Berkeley systems laid it out on the stack's
 * side, with its own length in front, and the way every system since has laid
 * it out on this one, with nothing in front. And a socket is asked about at a
 * level numbered one way there and another way here. Neither is deep, but
 * both are silent when got wrong, so both are done in one place.
 *
 * A descriptor of the stack is a descriptor of this system too: the stack is
 * told to number its own from where this system stops numbering, so which of
 * the two a descriptor belongs to is a matter of which side of that line it
 * falls, and everything that takes one knows where to send it.
 */

#include <errno.h>
#include <stddef.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>

#include <arch/port.h>
#include <kernel/errno.h>
#include <kernel/syscall.h>

#include "kconfig.h"

#ifdef CONFIG_NET

/* Declared rather than included: the stack's own header lays a socket address
 * out differently, and the two shapes must not meet in one file
 */
int lwip_socket(int domain, int type, int protocol);
int lwip_close(int s);
int lwip_bind(int s, const void *name, socklen_t namelen);
int lwip_connect(int s, const void *name, socklen_t namelen);
int lwip_listen(int s, int backlog);
int lwip_accept(int s, void *addr, socklen_t *addrlen);
int lwip_shutdown(int s, int how);
int lwip_getsockname(int s, void *name, socklen_t *namelen);
int lwip_getpeername(int s, void *name, socklen_t *namelen);
int lwip_setsockopt(int s,
                    int level,
                    int optname,
                    const void *optval,
                    socklen_t optlen);
int lwip_getsockopt(int s,
                    int level,
                    int optname,
                    void *optval,
                    socklen_t *optlen);
ssize_t lwip_send(int s, const void *dataptr, size_t size, int flags);
ssize_t lwip_recv(int s, void *mem, size_t len, int flags);
ssize_t lwip_sendto(int s,
                    const void *dataptr,
                    size_t size,
                    int flags,
                    const void *to,
                    socklen_t tolen);
ssize_t lwip_recvfrom(int s,
                      void *mem,
                      size_t len,
                      int flags,
                      void *from,
                      socklen_t *fromlen);

/* What the stack numbers the level of a socket's own options, which is not
 * what this system numbers it
 */
#define LWIP_SOL_SOCKET 0xfff

/* The largest address either shape needs */
#define SOCKADDR_MAX 28

/*==================================*
 * Between the two shapes of address *
 *==================================*/

/* This system puts a two byte family at the front. The stack puts a length
 * there and a one byte family after it. Everything past those two bytes is
 * the same in both, so only the two are touched
 */
static socklen_t addr_out(const struct sockaddr *from,
                          socklen_t len,
                          unsigned char *to)
{
    if (!from || len < 2 || len > SOCKADDR_MAX)
        return 0;

    memcpy(to, from, len);
    to[0] = (unsigned char) len;
    to[1] = (unsigned char) from->sa_family;

    return len;
}

static void addr_in(const unsigned char *from,
                    socklen_t len,
                    struct sockaddr *to,
                    socklen_t *tolen)
{
    if (!to || !tolen)
        return;

    socklen_t room = *tolen;
    socklen_t copy = len < room ? len : room;

    if (copy >= 2) {
        memcpy(to, from, copy);
        to->sa_family = from[1];
    }

    *tolen = len;
}

/* The two levels this system numbers differently from the stack. Anything
 * else is a protocol's own level, and those two agree
 */
static int level_out(int level)
{
    return level == SOL_SOCKET ? LWIP_SOL_SOCKET : level;
}

/*==================================*
 * The calls themselves              *
 *==================================*/

/* Both of these are the kernel's: one makes a socket and wraps it in a file,
 * the other says which socket a file stands for
 */
static NACKED int __socket_wrap(int lwip_fd)
{
    SYSCALL(SOCKET_WRAP);
}

static NACKED int __socket_lwip_fd(int fd)
{
    SYSCALL(SOCKET_LWIP_FD);
}

/* The socket is made here rather than in the kernel: making one is asking the
 * stack, and the stack answers only a thread. The kernel is then asked for a
 * descriptor to put it behind
 */
int socket(int domain, int type, int protocol)
{
    int s = lwip_socket(domain, type, protocol);

    if (s < 0)
        return -1;

    int fd = set_errno(__socket_wrap(s));

    if (fd < 0) {
        lwip_close(s);
        return -1;
    }

    return fd;
}

/* Turn the descriptor the caller holds into the socket the stack knows, or
 * fail the way POSIX fails a descriptor that is not one
 */
static int sock_of(int fd)
{
    int s = __socket_lwip_fd(fd);

    if (s < 0) {
        errno = ENOTSOCK;
        return -1;
    }

    return s;
}

int bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen)
{
    int _s = sock_of(sockfd);
    if (_s < 0)
        return -1;

    unsigned char buf[SOCKADDR_MAX];
    socklen_t len = addr_out(addr, addrlen, buf);

    if (!len) {
        errno = EINVAL;
        return -1;
    }

    return lwip_bind(_s, buf, len);
}

int connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen)
{
    int _s = sock_of(sockfd);
    if (_s < 0)
        return -1;

    unsigned char buf[SOCKADDR_MAX];
    socklen_t len = addr_out(addr, addrlen, buf);

    if (!len) {
        errno = EINVAL;
        return -1;
    }

    return lwip_connect(_s, buf, len);
}

int listen(int sockfd, int backlog)
{
    int _s = sock_of(sockfd);
    if (_s < 0)
        return -1;

    return lwip_listen(_s, backlog);
}

int accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen)
{
    int _s = sock_of(sockfd);
    if (_s < 0)
        return -1;

    unsigned char buf[SOCKADDR_MAX];
    socklen_t len = sizeof(buf);

    int fd = lwip_accept(_s, addr ? buf : NULL, addr ? &len : NULL);

    if (fd >= 0 && addr)
        addr_in(buf, len, addr, addrlen);

    return fd;
}

int shutdown(int sockfd, int how)
{
    int _s = sock_of(sockfd);
    if (_s < 0)
        return -1;

    return lwip_shutdown(_s, how);
}

int getsockname(int sockfd, struct sockaddr *addr, socklen_t *addrlen)
{
    int _s = sock_of(sockfd);
    if (_s < 0)
        return -1;

    unsigned char buf[SOCKADDR_MAX];
    socklen_t len = sizeof(buf);

    int err = lwip_getsockname(_s, buf, &len);

    if (!err)
        addr_in(buf, len, addr, addrlen);

    return err;
}

int getpeername(int sockfd, struct sockaddr *addr, socklen_t *addrlen)
{
    int _s = sock_of(sockfd);
    if (_s < 0)
        return -1;

    unsigned char buf[SOCKADDR_MAX];
    socklen_t len = sizeof(buf);

    int err = lwip_getpeername(_s, buf, &len);

    if (!err)
        addr_in(buf, len, addr, addrlen);

    return err;
}

int setsockopt(int sockfd,
               int level,
               int optname,
               const void *optval,
               socklen_t optlen)
{
    int _s = sock_of(sockfd);
    if (_s < 0)
        return -1;

    return lwip_setsockopt(_s, level_out(level), optname, optval, optlen);
}

int getsockopt(int sockfd,
               int level,
               int optname,
               void *optval,
               socklen_t *optlen)
{
    int _s = sock_of(sockfd);
    if (_s < 0)
        return -1;

    return lwip_getsockopt(_s, level_out(level), optname, optval, optlen);
}

ssize_t send(int sockfd, const void *buf, size_t len, int flags)
{
    int _s = sock_of(sockfd);
    if (_s < 0)
        return -1;

    return lwip_send(_s, buf, len, flags);
}

ssize_t recv(int sockfd, void *buf, size_t len, int flags)
{
    int _s = sock_of(sockfd);
    if (_s < 0)
        return -1;

    return lwip_recv(_s, buf, len, flags);
}

ssize_t sendto(int sockfd,
               const void *buf,
               size_t len,
               int flags,
               const struct sockaddr *dest_addr,
               socklen_t addrlen)
{
    int _s = sock_of(sockfd);
    if (_s < 0)
        return -1;

    if (!dest_addr)
        return lwip_sendto(_s, buf, len, flags, NULL, 0);

    unsigned char addr[SOCKADDR_MAX];
    socklen_t alen = addr_out(dest_addr, addrlen, addr);

    if (!alen) {
        errno = EINVAL;
        return -1;
    }

    return lwip_sendto(_s, buf, len, flags, addr, alen);
}

ssize_t recvfrom(int sockfd,
                 void *buf,
                 size_t len,
                 int flags,
                 struct sockaddr *src_addr,
                 socklen_t *addrlen)
{
    int _s = sock_of(sockfd);
    if (_s < 0)
        return -1;

    unsigned char addr[SOCKADDR_MAX];
    socklen_t alen = sizeof(addr);

    ssize_t got = lwip_recvfrom(_s, buf, len, flags, src_addr ? addr : NULL,
                                src_addr ? &alen : NULL);

    if (got >= 0 && src_addr)
        addr_in(addr, alen, src_addr, addrlen);

    return got;
}

#else /* CONFIG_NET */

/* Built without a network. The calls are here so that a program which asks
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

#endif /* CONFIG_NET */
