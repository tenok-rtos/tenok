#ifndef __MPOOL_H__
#define __MPOOL_H__

struct memory_pool {
	int     offset;
	size_t  size;
	uint8_t *mem;
};

void memory_pool_init(struct memory_pool *mem_pool, size_t size, uint8_t *mem);
void *memory_pool_alloc(struct memory_pool *mem_pool, size_t size);

#endif
