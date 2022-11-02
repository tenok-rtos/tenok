#include <stddef.h>
#include <stdint.h>
#include "mpool.h"

void memory_pool_init(struct memory_pool *mem_pool, size_t size, uint8_t *mem)
{
	mem_pool->offset = 0;
	mem_pool->size = size;
	mem_pool->mem = mem;
}

void *memory_pool_alloc(struct memory_pool *mem_pool, size_t size)
{
	/* test if size fits */
	if((mem_pool->offset + size) <= mem_pool->size) {
		uint8_t *alloc = mem_pool->mem + mem_pool->offset;
		mem_pool->offset += size;
		return alloc;
	}

	return NULL;
}
