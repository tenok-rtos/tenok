#ifndef __MPOOL_H__
#define __MPOOL_H__

#include <kernel/spinlock.h>

struct memory_pool {
    spinlock_t lock;

    int     offset;
    size_t  size;
    uint8_t *mem;
};

void memory_pool_init(struct memory_pool *mem_pool, uint8_t *mem, size_t size);
void *memory_pool_alloc(struct memory_pool *mem_pool, size_t size);

#endif
