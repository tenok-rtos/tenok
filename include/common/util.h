/**
 * @file
 */
#ifndef __UTIL_H__
#define __UTIL_H__

#include <stdint.h>

#define CEILING(x, y) (((x) + (y) -1) / (y))

#define BITMAP_SIZE(x) CEILING(x, 32) /* Bitmap type should be uint32_t */

#define ALIGN_MASK(x, mask) ((x) & ~(mask))
#define ALIGN(x, a) ALIGN_MASK(x, (__typeof__(x)) ((a) -1))

#endif
