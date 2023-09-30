/**
 * @file
 */
#ifndef __MM_H__
#define __MM_H__

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

#endif
