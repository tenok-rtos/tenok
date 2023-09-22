#include <stddef.h>
#include <string.h>

#include <mm/slab.h>
#include <kernel/page.h>
#include <kernel/list.h>
#include <kernel/util.h>
#include <kernel/bitops.h>

#define OBJS_PER_SLAB(objsize) \
    ((CACHE_PAGE_SIZE - sizeof(struct slab) / objsize))

static struct kmem_cache cache_caches = {
    .objsize = sizeof(struct kmem_cache),
    .objnum = OBJS_PER_SLAB(sizeof(struct kmem_cache)),
    .name = "cache-cache",
    .slabs_free = LIST_HEAD_INIT(cache_caches.slabs_free),
    .slabs_partial = LIST_HEAD_INIT(cache_caches.slabs_partial),
    .slabs_full = LIST_HEAD_INIT(cache_caches.slabs_full),
    .alloc_succeed = 0,
    .alloc_fail = 0,
    .opts = CACHE_OPT_NONE
};

static LIST_HEAD(caches); //list of the caches

static inline struct slab *get_slab_from_obj(void *obj, size_t page_size)
{
    /* calculate the slab address from object by doing round-down alignment */
    return (struct slab *)align((unsigned long)obj, page_size);
}

static inline int obj_index_in_slab(void *obj, struct kmem_cache *cache)
{
    /* mask out the base address and divide it by the object size */
    return (((unsigned long)obj & ((1 << 8) - 1)) - sizeof(struct slab)) / cache->objsize;
}

struct kmem_cache *kmem_cache_create(const char *name, size_t size, size_t align,
                                     unsigned long flags, void (*ctor)(void *))
{
    struct kmem_cache *cache;

    /* too small */
    if(size < 8) {
        return NULL;
    }

    /* too big */
    if(size > (256 - sizeof(struct slab) / 2)) {
        return NULL;
    }

    /* attempt to allocate new cache memory */
    cache = kmem_cache_alloc(&cache_caches, 0);

    /* failed to allocate cache memory */
    if(!cache) {
        return NULL;
    }

    /* initialize the new cache */
    cache->objsize = size;
    cache->objnum = OBJS_PER_SLAB(size);
    cache->opts = CACHE_OPT_NONE;
    strncpy(cache->name, name, CACHE_NAME_LEN);
    list_init(&cache->slabs_free);
    list_init(&cache->slabs_partial);
    list_init(&cache->slabs_full);
    list_push(&cache->list, &caches);
}

static struct slab *kmem_cache_grow(struct kmem_cache *cache)
{
    /* attempt to allocate a 256-bytes page (order = 0) */
    struct slab*slab = alloc_pages(0);

    /* failed to allocate new page */
    if(!slab) {
        return NULL;
    }

    /* initialize the new slab */
    slab->free_bitmap[0] = 0;
    slab->free_objects = cache->objnum;
    list_push(&cache->slabs_free, &slab->list);

    /* return the address of new slab */
    return slab;
}

void *kmem_cache_alloc(struct kmem_cache *cache, unsigned long flags)
{
    struct slab *slab = NULL;
    void *mem;

    /* check if the partial list contains space for new slab */
    if(list_is_empty(&cache->slabs_partial)) {
        /* no, check if the free list contains space for new slab */
        if(list_is_empty(&cache->slabs_free)) {
            /* no, grow the cache by allocating new page */
            cache->alloc_fail++;
            slab = kmem_cache_grow(cache);

            /* failed to grow the cache by allocating new page */
            if(!slab) {
                return NULL;
            }
        } else {
            /* yes, acquire the new slab from the free list */
            cache->alloc_succeed++;
            slab = list_first_entry(&cache->slabs_partial, struct slab, list);
        }

        /* move the slab into the partial list */
        list_move(&slab->list, &cache->slabs_partial);
    } else {
        /* yes, obtain the slab from the partial list */
        cache->alloc_succeed++;
        slab = list_first_entry(&cache->slabs_partial, struct slab, list);
    }

    /* mark the bitmap of the slab */
    int bit = find_first_zero_bit(slab->free_bitmap, cache->objnum);
    bitmap_set_bit(slab->free_bitmap, bit);

    /* calculate the memory of the acquired slab memory */
    mem = slab->data + bit * cache->objsize;

    /* update free objects count of the slab */
    slab->free_objects--;

    /* move the slab into the full list if the page has no space for
     * more new slabs */
    if(!slab->free_objects) {
        list_move(&slab->list, &cache->slabs_full);
    }

    /* return the address of the allocated slab memory */
    return mem;
}

static int slab_destroy(struct kmem_cache *cache, struct slab *slab)
{
    /* remove the slab from its current list and free the page */
    list_remove(&slab->list);
    free_pages((unsigned long) slab, 0);

    return 0;
}

void kmem_cache_free(struct kmem_cache *cache, void *obj)
{
    struct slab *slab;
    int bit;

    /* obtain the slab from object and its bitmap index */
    slab = get_slab_from_obj(obj, 256);
    bit = obj_index_in_slab(obj, cache);

    /* reset the bitmap of the object in the slab */
    bitmap_clear_bit(slab->free_bitmap, bit);

    /* update free objects count of the slab */
    slab->free_objects++;

    /* check the free object count of the slab */
    if(slab->free_objects == cache->objnum) {
        /* free the whole page since it contains no more slab */
        slab_destroy(cache, slab);
    } else if(slab->free_objects == 1) {
        /* move the slab from full list into the partial list */
        list_move(&slab->list, &cache->slabs_partial);
    }
}

void kmem_cache_init(void)
{
    /* add the cache-cache into the cache list */
    list_push(&caches, &cache_caches.list);

    /* allocate space for the cache-cache */
    kmem_cache_grow(&cache_caches);
}
