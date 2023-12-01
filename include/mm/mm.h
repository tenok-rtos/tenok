/**
 * @file
 */
#ifndef __MM_H__
#define __MM_H__

#include <stddef.h>
#include <stdint.h>

#define DEF_KMALLOC_SLAB(_size)                  \
    {                                            \
        .size = _size, .name = "kmalloc-" #_size \
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
 * @brief  Allocate a memory space for using in the kernel
 * @param  size: The size of the memory in bytes.
 * @retval None
 */
void *kmalloc(size_t size);

/**
 * @brief  Free the memory space pointed to by ptr
 * @param  ptr: Pointer to the allocated memory.
 * @retval None
 */
void kfree(void *ptr);

unsigned long heap_get_total_size(void);
unsigned long heap_get_free_size(void);
void heap_init(void);
void *__malloc(size_t size);
void __free(void *ptr);

#endif
