#ifndef _TENOK_NETDB_H
#define _TENOK_NETDB_H

#include <sys/socket.h>

struct addrinfo {
    int ai_flags;
    int ai_family;
    int ai_socktype;
    int ai_protocol;
    socklen_t ai_addrlen;
    struct sockaddr *ai_addr;
    char *ai_canonname;
    struct addrinfo *ai_next;
};

struct hostent {
    char *h_name;
    char **h_aliases;
    int h_addrtype;
    int h_length;
    char **h_addr_list;
};

#define AI_CANONNAME 0x0002
#define NI_NUMERICHOST 0x0001
#define NI_NUMERICSERV 0x0002
#define EAI_NONAME -2

#endif
