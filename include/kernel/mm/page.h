#ifndef __PAGE_H__
#define __PAGE_H__

#include "kconfig.h"

#if PAGE_SIZE_SELECT == PAGE_SIZE_32K
#define PAGE_ORDER_MAX 4
#elif PAGE_SIZE_SELECT == PAGE_SIZE_64K
#define PAGE_ORDER_MAX 5
#endif

#define PAGE_SIZE_MIN  256

unsigned long get_page_total_size(void);
unsigned long get_page_total_free_size(void);
void *alloc_pages(unsigned long order);
void free_pages(unsigned long addr, unsigned long order);
long size_to_page_order(unsigned long size);
unsigned long page_order_to_size(long order);

#endif
