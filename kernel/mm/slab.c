#include <stddef.h>
#include <string.h>

#include <common/bitops.h>
#include <common/list.h>
#include <common/util.h>
#include <mm/page.h>
#include <mm/slab.h>

#define OBJS_PER_SLAB(page_size, objsize) \
    ((page_size - sizeof(struct slab)) / objsize)

/* Cache of caches */
static struct kmem_cache cache_caches = {
    .objsize = sizeof(struct kmem_cache),
    .objnum = OBJS_PER_SLAB(PAGE_SIZE_MIN, sizeof(struct kmem_cache)),
    .name = "cache-cache",
    .slabs_free = LIST_HEAD_INIT(cache_caches.slabs_free),
    .slabs_partial = LIST_HEAD_INIT(cache_caches.slabs_partial),
    .slabs_full = LIST_HEAD_INIT(cache_caches.slabs_full),
    .alloc_succeed = 0,
    .alloc_fail = 0,
    .opts = CACHE_OPT_NONE,
};

/* Caches list */
static LIST_HEAD(caches);

static inline struct slab *get_slab_from_obj(void *obj, size_t page_size)
{
    /* Calculate the slab address from object by doing round-down alignment */
    return (struct slab *) ALIGN((unsigned long) obj, page_size);
}

static inline int obj_index_in_slab(void *obj, struct kmem_cache *cache)
{
    /* Mask out the base address and divide it by the object size */
    return (((unsigned long) obj & ((1 << 8) - 1)) - sizeof(struct slab)) /
           cache->objsize;
}

struct kmem_cache *kmem_cache_create(const char *name,
                                     size_t size,
                                     size_t align,
                                     unsigned long flags,
                                     void (*ctor)(void *))
{
    struct kmem_cache *cache;

    /* Find a suitable page order for the slab. one
     * single page should at least contains 2 slabs */
    int order = size_to_page_order(size);
    int page_size = page_order_to_size(order);
    int objnum = OBJS_PER_SLAB(page_size, size);

    while (objnum < 2 && order <= PAGE_ORDER_MAX) {
        order++;
        page_size = page_order_to_size(order);
        objnum = OBJS_PER_SLAB(page_size, size);
    }

    /* The request size is too large */
    if (order > PAGE_ORDER_MAX)
        return NULL;

    /* Allocate new cache memory */
    cache = kmem_cache_alloc(&cache_caches, 0);

    /* Failed to allocate cache memory */
    if (!cache)
        return NULL;

    /* Initialize the new cache */
    cache->objsize = size;
    cache->objnum = objnum;
    cache->page_order = order;
    cache->opts = CACHE_OPT_NONE;
    strncpy(cache->name, name, CACHE_NAME_LEN);
    INIT_LIST_HEAD(&cache->slabs_free);
    INIT_LIST_HEAD(&cache->slabs_partial);
    INIT_LIST_HEAD(&cache->slabs_full);
    list_add(&cache->list, &caches);

    return cache;
}

static struct slab *kmem_cache_grow(struct kmem_cache *cache)
{
    /* Allocate a new page for the slab */
    struct slab *slab = alloc_pages(cache->page_order);

    /* Failed to allocate new page */
    if (!slab) {
        return NULL;
    }

    /* Initialize the new slab */
    slab->free_bitmap[0] = 0;
    slab->free_objects = cache->objnum;
    list_add(&slab->list, &cache->slabs_free);

    /* Return the address of new slab */
    return slab;
}

void *kmem_cache_alloc(struct kmem_cache *cache, unsigned long flags)
{
    struct slab *slab = NULL;
    void *mem;

    /* Check if the partial list contains space for new slab */
    if (list_empty(&cache->slabs_partial)) {
        /* No, check if the free list contains space for new slab */
        if (list_empty(&cache->slabs_free)) {
            /* No, grow the cache by allocating new page */
            cache->alloc_fail++;
            slab = kmem_cache_grow(cache);

            /* Failed to grow the cache by allocating new page */
            if (!slab) {
                return NULL;
            }
        } else {
            /* Yes, acquire the new slab from the free list */
            cache->alloc_succeed++;
            slab = list_first_entry(&cache->slabs_free, struct slab, list);
        }

        /* Move the slab into the partial list */
        list_move(&slab->list, &cache->slabs_partial);
    } else {
        /* Yes, obtain the slab from the partial list */
        cache->alloc_succeed++;
        slab = list_first_entry(&cache->slabs_partial, struct slab, list);
    }

    /* Mark the bitmap of the slab */
    int bit = find_first_zero_bit(slab->free_bitmap, cache->objnum);
    bitmap_set_bit(slab->free_bitmap, bit);

    /* Calculate the memory of the acquired slab memory */
    mem = slab->data + bit * cache->objsize;

    /* Update free objects count of the slab */
    slab->free_objects--;

    /* Move the slab into the full list if the page has no space for
     * more new slabs */
    if (!slab->free_objects) {
        list_move(&slab->list, &cache->slabs_full);
    }
    /* Return the address of the allocated slab memory */
    return mem;
}

static int slab_destroy(struct kmem_cache *cache, struct slab *slab)
{
    /* Remove the slab from its current list and free the page */
    list_del(&slab->list);
    free_pages((unsigned long) slab, 0);

    return 0;
}

void kmem_cache_free(struct kmem_cache *cache, void *obj)
{
    struct slab *slab;
    int bit;

    /* Acquire the slab from object and its bitmap index */
    slab = get_slab_from_obj(obj, 256);
    bit = obj_index_in_slab(obj, cache);

    /* Reset the bitmap of the object in the slab */
    bitmap_clear_bit(slab->free_bitmap, bit);

    /* Update free objects count of the slab */
    slab->free_objects++;

    /* Check the free object count of the slab */
    if (slab->free_objects == cache->objnum) {
        /* Free the whole page since it contains no more slab */
        slab_destroy(cache, slab);
    } else if (slab->free_objects == 1) {
        /* Move the slab from full list into the partial list */
        list_move(&slab->list, &cache->slabs_partial);
    }
}

void kmem_cache_init(void)
{
    /* Add the cache-cache into the cache list */
    list_add(&cache_caches.list, &caches);

    /* Allocate space for the cache-cache */
    kmem_cache_grow(&cache_caches);
}
