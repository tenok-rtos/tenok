/**
 * @file
 * Naming an interface.
 *
 * This system has the one, and nothing here is asked to find another. What is
 * declared is what programs written for larger systems expect to find before
 * they will compile, and it answers for the interface that exists.
 */
#ifndef _TENOK_NET_IF_H
#define _TENOK_NET_IF_H

#include <stddef.h>

#include <sys/socket.h>

#define IF_NAMESIZE 16
#define IFNAMSIZ IF_NAMESIZE

struct ifreq {
    char ifr_name[IFNAMSIZ];
    union {
        struct sockaddr ifru_addr;
        struct sockaddr ifru_netmask;
        struct sockaddr ifru_hwaddr;
        short ifru_flags;
        int ifru_ifindex;
        int ifru_mtu;
    } ifr_ifru;
};

#define ifr_addr ifr_ifru.ifru_addr
#define ifr_netmask ifr_ifru.ifru_netmask
#define ifr_hwaddr ifr_ifru.ifru_hwaddr
#define ifr_flags ifr_ifru.ifru_flags
#define ifr_ifindex ifr_ifru.ifru_ifindex
#define ifr_mtu ifr_ifru.ifru_mtu

unsigned int if_nametoindex(const char *ifname);
char *if_indextoname(unsigned int ifindex, char *ifname);

#endif
