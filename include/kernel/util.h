#ifndef __UTIL_H__
#define __UTIL_H__

#define align(x, a) align_mask(x, (__typeof__(x))((a) - 1))
#define align_mask(x, mask) ((x) & ~(mask))

#endif
