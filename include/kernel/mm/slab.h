#ifndef __SLAB_H__
#define __SLAB_H__

#include <kernel/list.h>

#define CACHE_PAGE_SIZE 256
#define CACHE_NAME_LEN  16

#define CACHE_OPT_NONE  0

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

struct kmem_cache *kmem_cache_create(const char *name, size_t size, size_t align,
                                     unsigned long flags, void (*ctor)(void *));
void *kmem_cache_alloc(struct kmem_cache *cache, unsigned long flags);
void kmem_cache_free(struct kmem_cache *cache, void *obj);
void kmem_cache_init(void);

#endif
