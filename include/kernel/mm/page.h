/**
 * @file
 */
#ifndef __PAGE_H__
#define __PAGE_H__

#include "kconfig.h"

#if PAGE_SIZE_SELECT == PAGE_SIZE_32K
#    define PAGE_ORDER_MAX 4
#elif PAGE_SIZE_SELECT == PAGE_SIZE_64K
#    define PAGE_ORDER_MAX 5
#endif

#define PAGE_SIZE_MIN  256


unsigned long get_page_total_size(void);
unsigned long get_page_total_free_size(void);

/**
 * @brief  Calculate the proper page order by giving memory
 *         size.
 * @param  size: The memory size in bytes.
 * @retval long: The page order.
 */
long size_to_page_order(unsigned long size);

/**
 * @brief  Calculate the page size by giving the page order
 * @param  long: The page order.
 * @retval unsigned long: The memory size of page in bytes.
 */
unsigned long page_order_to_size(long order);

/**
 * @brief  Allocate a new memory page
 * @param  long: The page order.
 * @retval void *: Null if the allocation failed; otherwise
 *                 the function returns the address of the
 *                 allocated memory page.
 */
void *alloc_pages(unsigned long order);

/**
 * @brief  Free an allocated memory page
 * @param  addr: The address of the memory page.
 * @param  order: The order of the memory page.
 * @retval None
 */
void free_pages(unsigned long addr, unsigned long order);

#endif
