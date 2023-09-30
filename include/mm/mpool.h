/**
 * @file
 */
#ifndef __MPOOL_H__
#define __MPOOL_H__

#include <kernel/spinlock.h>

struct memory_pool {
    spinlock_t lock;

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
void memory_pool_init(struct memory_pool *mem_pool, uint8_t *mem, size_t size);

/**
 * @brief  Allocate a memory space from the memory pool
 * @param  mem_pool: The memory pool object to provide.
 * @param  size: The size of the memory space to allocate.
 */
void *memory_pool_alloc(struct memory_pool *mem_pool, size_t size);

#endif
