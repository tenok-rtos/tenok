/**
 * @file
 */
#ifndef __SLAB_H__
#define __SLAB_H__

#include <common/list.h>

#define CACHE_PAGE_SIZE 256
#define CACHE_NAME_LEN 16

#define CACHE_OPT_NONE 0

struct kmem_cache {
    struct list_head slabs_free;
    struct list_head slabs_partial;
    struct list_head slabs_full;
    struct list_head list;
    unsigned short objsize;
    unsigned short objnum;
    unsigned short page_order;
    int opts;
    int alloc_succeed;
    int alloc_fail;
    char name[CACHE_NAME_LEN];
};

struct slab {
    unsigned long free_bitmap[1];
    int free_objects;
    struct list_head list;
    char data[0];
};

/**
 * @brief  Create a new slab cache. The function should only be
 *         called inside the kernel space
 * @param  name: Name of the cache.
 * @param  size: Size of the slab managed by the cache.
 * @param  align: Size of the slab memory should align to.
 * @param  flags: Not used.
 * @param  ctor: Not used.
 * @retval struct kmem_cache *: Pointer to the new created cache.
 */
struct kmem_cache *kmem_cache_create(const char *name,
                                     size_t size,
                                     size_t align,
                                     unsigned long flags,
                                     void (*ctor)(void *));

/**
 * @brief  Allocate a new slab memory. The function should only
 *         be called inside the kernel space
 * @param  cache: The cache object for managing the slabs.
 * @param  flags: Not used.
 * @retval void *: Pointer to the allocated memory.
 */
void *kmem_cache_alloc(struct kmem_cache *cache, unsigned long flags);

/**
 * @brief  Free the allocated slab memory. The function should
 *         only be called inside the kernel space
 * @param  cache: The cache object for managing the slabs.
 * @param  obj: The pointer to the allocated slab memory.
 * @retval None
 */
void kmem_cache_free(struct kmem_cache *cache, void *obj);

void kmem_cache_init(void);

#endif
