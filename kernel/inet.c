/**
 * @file
 * An address as a number and an address as text, each turned into the other.
 *
 * None of this asks anything of a network. It is arithmetic on four bytes and
 * belongs to the system whether or not one is built in.
 */

#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>

int inet_aton(const char *cp, struct in_addr *addr)
{
    unsigned int part[4];
    int n = 0;

    if (!cp)
        return 0;

    /* Only the four part form is taken. The shorter forms the older systems
     * allowed are a source of surprise and nothing here writes them
     */
    while (n < 4) {
        if (*cp < '0' || *cp > '9')
            return 0;

        unsigned int v = 0;
        int digits = 0;

        while (*cp >= '0' && *cp <= '9') {
            v = v * 10 + (unsigned int) (*cp++ - '0');
            if (++digits > 3 || v > 255)
                return 0;
        }

        part[n++] = v;

        if (n < 4) {
            if (*cp != '.')
                return 0;
            cp++;
        }
    }

    if (*cp != '\0')
        return 0;

    if (addr)
        addr->s_addr =
            htonl((part[0] << 24) | (part[1] << 16) | (part[2] << 8) | part[3]);

    return 1;
}

in_addr_t inet_addr(const char *cp)
{
    struct in_addr addr;

    return inet_aton(cp, &addr) ? addr.s_addr : INADDR_NONE;
}

/* POSIX has this answer from storage of its own, which is why it may only be
 * read before the next call
 */
char *inet_ntoa(struct in_addr in)
{
    static char text[INET_ADDRSTRLEN];
    unsigned int a = ntohl(in.s_addr);

    snprintf(text, sizeof(text), "%u.%u.%u.%u", (a >> 24) & 0xff,
             (a >> 16) & 0xff, (a >> 8) & 0xff, a & 0xff);

    return text;
}

const char *inet_ntop(int af, const void *src, char *dst, socklen_t size)
{
    if (af != AF_INET || !src || !dst)
        return NULL;

    const struct in_addr *in = src;
    unsigned int a = ntohl(in->s_addr);

    if (snprintf(dst, size, "%u.%u.%u.%u", (a >> 24) & 0xff, (a >> 16) & 0xff,
                 (a >> 8) & 0xff, a & 0xff) >= (int) size)
        return NULL;

    return dst;
}

int inet_pton(int af, const char *src, void *dst)
{
    if (af != AF_INET)
        return -1;

    return inet_aton(src, dst);
}
