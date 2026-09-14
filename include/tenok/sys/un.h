/**
 * @file
 * The address of a socket that goes no further than this machine.
 *
 * This system has no such socket. The shape is declared because programs
 * written for systems that do will not compile without it, and a program that
 * asks for one is told there is none when it asks, not when it is built.
 */
#ifndef _TENOK_SYS_UN_H
#define _TENOK_SYS_UN_H

#include <sys/socket.h>

#define UNIX_PATH_MAX 108

struct sockaddr_un {
    sa_family_t sun_family;
    char sun_path[UNIX_PATH_MAX];
};

#endif
