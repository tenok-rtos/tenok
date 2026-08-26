/* Tenok has no network stack, only the types and constants are named */
#ifndef _TENOK_SYS_SOCKET_H
#define _TENOK_SYS_SOCKET_H

#include <sys/types.h>

#define AF_UNSPEC 0
#define AF_UNIX 1
#define AF_INET 2
#define AF_INET6 10
#define PF_UNSPEC AF_UNSPEC
#define PF_INET AF_INET
#define PF_INET6 AF_INET6

#define SOCK_STREAM 1
#define SOCK_DGRAM 2
#define SOCK_RAW 3
#define SOCK_RDM 4
#define SOCK_SEQPACKET 5

#define SOL_SOCKET 1
#define SHUT_RD 0
#define SHUT_WR 1
#define SHUT_RDWR 2

typedef unsigned short sa_family_t;

struct sockaddr {
    sa_family_t sa_family;
    char sa_data[14];
};

struct sockaddr_storage {
    sa_family_t ss_family;
    char __ss_padding[126];
};

struct in_addr {
    unsigned int s_addr;
};

struct sockaddr_in {
    sa_family_t sin_family;
    unsigned short sin_port;
    struct in_addr sin_addr;
    char sin_zero[8];
};

struct in6_addr {
    unsigned char s6_addr[16];
};

struct sockaddr_in6 {
    sa_family_t sin6_family;
    unsigned short sin6_port;
    unsigned int sin6_flowinfo;
    struct in6_addr sin6_addr;
    unsigned int sin6_scope_id;
};

struct msghdr {
    void *msg_name;
    socklen_t msg_namelen;
    struct iovec *msg_iov;
    int msg_iovlen;
    void *msg_control;
    socklen_t msg_controllen;
    int msg_flags;
};

/* Tenok carries no network, so a program that asks for one is told there is
 * none rather than being left to find out
 */

/**
 * @brief  Create an endpoint for communication.
 * @param  domain: The family the endpoint speaks in.
 * @param  type: How it carries what is sent.
 * @param  protocol: Which protocol of the family to use.
 * @retval int: -1 with errno set to ENOSYS.
 */
int socket(int domain, int type, int protocol);

/**
 * @brief  Give the endpoint the address it answers on.
 * @param  sockfd: The endpoint.
 * @param  addr: The address to answer on.
 * @param  addrlen: The size of the address.
 * @retval int: -1 with errno set to ENOSYS.
 */
int bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen);

/**
 * @brief  Let the endpoint take the connections that arrive.
 * @param  sockfd: The endpoint.
 * @param  backlog: How many may wait to be taken.
 * @retval int: -1 with errno set to ENOSYS.
 */
int listen(int sockfd, int backlog);

/**
 * @brief  Send a message to the address given.
 * @param  sockfd: The endpoint to send from.
 * @param  buf: What to send.
 * @param  len: How much of it.
 * @param  flags: How to send it.
 * @param  dest_addr: Where to send it.
 * @param  addrlen: The size of the address.
 * @retval ssize_t: -1 with errno set to ENOSYS.
 */
ssize_t sendto(int sockfd,
               const void *buf,
               size_t len,
               int flags,
               const struct sockaddr *dest_addr,
               socklen_t addrlen);

#endif
