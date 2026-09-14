/**
 * @file
 * What the stack is told about the machine it has been put on.
 */
#ifndef __LWIP_ARCH_CC_H__
#define __LWIP_ARCH_CC_H__

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define LWIP_NO_UNISTD_H 0

/* The system has this already, and the stack is told so rather than being let
 * declare a second one of its own
 */
#include <limits.h>
#include <sys/types.h>
#define SOCKLEN_T_DEFINED 1

/* The stack decides whether the system has a signed size by looking for this,
 * and this system has the type without saying so anywhere it would look
 */
#ifndef SSIZE_MAX
#define SSIZE_MAX 0x7fffffff
#endif

/* The part is little endian and its compiler packs the way the stack expects
 * when it is asked to
 */
#define BYTE_ORDER LITTLE_ENDIAN

#define PACK_STRUCT_BEGIN
#define PACK_STRUCT_STRUCT __attribute__((packed))
#define PACK_STRUCT_END
#define PACK_STRUCT_FIELD(x) x

/* Where the stack says something, and what it does when it finds the world is
 * not as it assumed. Stopping is not on offer: this runs a shell someone may
 * still want, so the complaint is made and the caller carries on
 */
#define LWIP_PLATFORM_DIAG(x) \
    do {                      \
        printf x;             \
    } while (0)

#define LWIP_PLATFORM_ASSERT(x)                                        \
    do {                                                               \
        printf("lwip: %s, at %s:%d\n", (x), __FILE__, (int) __LINE__); \
    } while (0)

#endif
