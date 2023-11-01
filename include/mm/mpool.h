/**
 * @file
 */
#ifndef __MPOOL_H__
#define __MPOOL_H__

#include <stddef.h>
#include <stdint.h>

struct mpool {
    int     offset;
    size_t  size;
    uint8_t *mem;
};

/**
 * @brief  Initialize a memory poll object with a contiguous memory space
 * @param  mem_pool: The memory poll object to provide.
 * @param  mem: The contiguous memory space to provide.
 * @param  size: The size of the provided memory space.
 */
void mpool_init(struct mpool *mem_pool, uint8_t *mem, size_t size);

/**
 * @brief  Allocate a memory space from the memory pool
 * @param  mem_pool: The memory pool object to provide.
 * @param  size: The size of the memory space to allocate.
 */
void *mpool_alloc(struct mpool *mem_pool, size_t size);

#endif
