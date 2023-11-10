#include <stddef.h>
#include <stdint.h>

#include <kernel/util.h>
#include <mm/mpool.h>

void mpool_init(struct mpool *mem_pool, uint8_t *mem, size_t size)
{
    mem_pool->offset = 0;
    mem_pool->size = size;
    mem_pool->mem = mem;
}
