/**
 * @file
 * Turning a name into an address, and an address into the text of one.
 *
 * The stack beside this system does the asking. What is done here is the same
 * turn of one shape into another that sockets need: the address the stack
 * hands back carries its own length in front of it, and this system's does
 * not.
 */

#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>

#include "kconfig.h"

#ifdef CONFIG_NET

/* Declared rather than included, for the same reason the socket calls are */
int lwip_getaddrinfo(const char *nodename,
                     const char *servname,
                     const void *hints,
                     void **res);
void lwip_freeaddrinfo(void *ai);
struct hostent *lwip_gethostbyname(const char *name);

/* The stack writes the length of the address into the first byte and the
 * family into the second. This system reads the family from the first two
 */
static void addr_in_place(struct sockaddr *addr)
{
    unsigned char *a = (unsigned char *) addr;

    if (!a)
        return;

    unsigned char family = a[1];

    a[0] = family;
    a[1] = 0;
}

int getaddrinfo(const char *node,
                const char *service,
                const struct addrinfo *hints,
                struct addrinfo **res)
{
    int err = lwip_getaddrinfo(node, service, hints, (void **) res);

    if (err)
        return err;

    for (struct addrinfo *ai = *res; ai; ai = ai->ai_next)
        addr_in_place(ai->ai_addr);

    return 0;
}

void freeaddrinfo(struct addrinfo *res)
{
    lwip_freeaddrinfo(res);
}

struct hostent *gethostbyname(const char *name)
{
    /* What this hands back carries addresses, not addresses of sockets, so
     * there is nothing in it laid out the way that would need turning round
     */
    return lwip_gethostbyname(name);
}

/* Only the numeric answer is given. Asking the other way round -- what name
 * an address goes by -- would need the stack to ask a question this system
 * has nothing to do with the answer to
 */
int getnameinfo(const struct sockaddr *addr,
                socklen_t addrlen,
                char *host,
                socklen_t hostlen,
                char *serv,
                socklen_t servlen,
                int flags)
{
    if (!addr || addr->sa_family != AF_INET)
        return EAI_FAMILY;

    if (flags & NI_NAMEREQD)
        return EAI_NONAME;

    const struct sockaddr_in *in = (const struct sockaddr_in *) addr;

    if (host && hostlen) {
        unsigned int a = ntohl(in->sin_addr.s_addr);

        if (snprintf(host, hostlen, "%u.%u.%u.%u", (a >> 24) & 0xff,
                     (a >> 16) & 0xff, (a >> 8) & 0xff,
                     a & 0xff) >= (int) hostlen)
            return EAI_OVERFLOW;
    }

    if (serv && servlen) {
        if (snprintf(serv, servlen, "%u", ntohs(in->sin_port)) >= (int) servlen)
            return EAI_OVERFLOW;
    }

    return 0;
}

const char *gai_strerror(int errcode)
{
    switch (errcode) {
    case 0:
        return "no error";
    case EAI_NONAME:
        return "name or service not known";
    case EAI_AGAIN:
        return "temporary failure in name resolution";
    case EAI_FAIL:
        return "non-recoverable failure in name resolution";
    case EAI_FAMILY:
        return "address family not supported";
    case EAI_SOCKTYPE:
        return "socket type not supported";
    case EAI_SERVICE:
        return "service not supported for this socket type";
    case EAI_MEMORY:
        return "memory allocation failure";
    case EAI_OVERFLOW:
        return "argument buffer overflow";
    default:
        return "unknown error";
    }
}

#endif /* CONFIG_NET */
