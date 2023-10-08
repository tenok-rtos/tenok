/**
 * @file
 */
#ifndef __UTIL_H__
#define __UTIL_H__

#include <stdint.h>

#define CEILING(x, y) (((x) + (y) - 1) / (y))

#define BITMAP_SIZE(x) CEILING(x, 32) /* bitmap type should be uint32_t */

#define align(x, a) align_mask(x, (__typeof__(x))((a) - 1))
#define align_mask(x, mask) ((x) & ~(mask))

static inline uintptr_t align_up(uintptr_t sz, size_t alignment)
{
    uintptr_t mask = alignment - 1;
    if ((alignment & mask) == 0) { /* power of two? */
        return (sz + mask) & ~mask;
    }
    return (((sz + mask) / alignment) * alignment);
}

#endif
