/**
 * @file
 */
#ifndef __MM_H__
#define __MM_H__

#include <stddef.h>
#include <stdint.h>

#define DEF_KMALLOC_SLAB(_size) { \
        .size = _size, \
        .name = "kmalloc-" #_size \
    }

#define KMALLOC_SLAB_TABLE_SIZE \
    (sizeof(kmalloc_slab_info) / sizeof(struct kmalloc_slab_info))

struct kmalloc_header {
    size_t size;
};

struct kmalloc_slab_info {
    char *name;
    size_t size;
};

/**
 * @brief  Allocate size bytes and returns a pointer to the allocated
           memory. The function can be only used in kernel
 * @param  size: The number of bytes to allocate for the memory.
 * @retval None
 */
void *kmalloc(size_t size);

/**
 * @brief  free the memory space pointed to by ptr, which must have
           been returned by a previous call to  malloc(). The function
           can be only used in kernel
 * @param  ptr: The memory space to provide.
 * @retval None
 */
void kfree(void *ptr);

unsigned long heap_get_total_size(void);
unsigned long heap_get_free_size(void);
void malloc_heap_init(void);
void *__malloc(size_t size);
void __free(void *ptr);

#endif
