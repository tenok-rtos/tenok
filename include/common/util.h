/**
 * @file
 */
#ifndef __UTIL_H__
#define __UTIL_H__

#include <stdint.h>

#define CEILING(x, y) (((x) + (y) -1) / (y))

#define BITMAP_SIZE(x) CEILING(x, 32) /* Bitmap type should be uint32_t */

#define ALIGN_MASK(x, mask) ((x) & ~(mask))
/* Round down to the given alignment */
#define ALIGN(x, a) ALIGN_MASK(x, (__typeof__(x)) ((a) -1))
/* Round up to the given alignment */
#define ALIGN_UP(x, a) ALIGN((x) + (__typeof__(x)) ((a) -1), a)

#endif
