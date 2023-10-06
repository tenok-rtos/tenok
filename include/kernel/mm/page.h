#ifndef __PAGE_H__
#define __PAGE_H__

#define PAGE_ORDER_MAX 4
#define PAGE_SIZE_MIN  256

void *alloc_pages(unsigned long order);
void free_pages(unsigned long addr, unsigned long order);
long size_to_page_order(unsigned long size);

#endif
