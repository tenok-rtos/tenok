#include <mpool.h>
#include <stddef.h>
#include <stdint.h>

#include <common/util.h>

void mpool_init(struct mpool *mpool, uint8_t *mem, size_t size)
{
    mpool->offset = 0;
    mpool->size = size;
    mpool->mem = mem;
}
