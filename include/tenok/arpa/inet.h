/**
 * @file
 * Turning an address between the number it is and the text it is written as.
 */
#ifndef _TENOK_ARPA_INET_H
#define _TENOK_ARPA_INET_H

#include <netinet/in.h>
#include <sys/socket.h>

in_addr_t inet_addr(const char *cp);
int inet_aton(const char *cp, struct in_addr *addr);
char *inet_ntoa(struct in_addr in);
const char *inet_ntop(int af, const void *src, char *dst, socklen_t size);
int inet_pton(int af, const char *src, void *dst);

#endif
