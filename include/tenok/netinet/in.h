/**
 * @file
 * The internet protocol, as the address of a socket sees it.
 */
#ifndef _TENOK_NETINET_IN_H
#define _TENOK_NETINET_IN_H

#include <stdint.h>
#include <sys/socket.h>

typedef uint16_t in_port_t;
typedef uint32_t in_addr_t;

#define IPPROTO_IP 0
#define IPPROTO_ICMP 1
#define IPPROTO_TCP 6
#define IPPROTO_UDP 17
#define IPPROTO_IPV6 41
#define IPPROTO_ICMPV6 58
#define IPPROTO_RAW 255

#define INADDR_ANY ((in_addr_t) 0x00000000)
#define INADDR_LOOPBACK ((in_addr_t) 0x7f000001)
#define INADDR_BROADCAST ((in_addr_t) 0xffffffff)
#define INADDR_NONE ((in_addr_t) 0xffffffff)

/* Options of the protocol itself, which are asked for at its own level
 * rather than at the socket's
 */
#define IP_TOS 1
#define IP_TTL 2
#define IP_MULTICAST_IF 32
#define IP_MULTICAST_TTL 33
#define IP_MULTICAST_LOOP 34
#define IP_ADD_MEMBERSHIP 35
#define IP_DROP_MEMBERSHIP 36

#define INET_ADDRSTRLEN 16
#define INET6_ADDRSTRLEN 46

/* The part is little endian and the wire is not, so every one of these turns
 * the bytes around
 */
static inline uint16_t htons(uint16_t x)
{
    return (uint16_t) ((x << 8) | (x >> 8));
}

static inline uint16_t ntohs(uint16_t x)
{
    return htons(x);
}

static inline uint32_t htonl(uint32_t x)
{
    return ((x & 0x000000ffU) << 24) | ((x & 0x0000ff00U) << 8) |
           ((x & 0x00ff0000U) >> 8) | ((x & 0xff000000U) >> 24);
}

static inline uint32_t ntohl(uint32_t x)
{
    return htonl(x);
}

#endif
