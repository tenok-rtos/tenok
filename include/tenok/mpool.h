/**
 * @file
 */
#ifndef __MPOOL_H__
#define __MPOOL_H__

#include <stddef.h>
#include <stdint.h>

struct mpool {
    int offset;
    size_t size;
    uint8_t *mem;
};

/**
 * @brief  Initialize a memory poll object with a contiguous memory space
 * @param  mem_pool: Pointer to the memory poll object.
 * @param  mem: A contiguous memory space to provide.
 * @param  size: The size of the memory poll.
 */
void mpool_init(struct mpool *mpool, uint8_t *mem, size_t size);

/**
 * @brief  Allocate a memory space from the memory pool
 * @param  mem_pool: Pointer to the memory poll object.
 * @param  size: The size of the memory to allocate in bytes.
 */
void *mpool_alloc(struct mpool *mpool, size_t size);

#endif
