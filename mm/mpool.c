#include <stddef.h>
#include <stdint.h>
#include "mpool.h"

void memory_pool_init(struct memory_pool *mem_pool, uint8_t *mem, size_t size)
{
	mem_pool->offset = 0;
	mem_pool->size = size;
	mem_pool->mem = mem;
	mem_pool->lock = 0;
}

void *memory_pool_alloc(struct memory_pool *mem_pool, size_t size)
{
	/* start of the critical section */
	spin_lock(&mem_pool->lock);

	void *addr = NULL;

	/* test if size fits */
	if((mem_pool->offset + size) <= mem_pool->size) {
		uint8_t *alloc = mem_pool->mem + mem_pool->offset;
		mem_pool->offset += size;
		addr = alloc;
	}

	/* end of the critical section */
	spin_unlock(&mem_pool->lock);

	return addr;
}
