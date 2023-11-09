#include <stdint.h>

#include <mm/page.h>
#include <kernel/bitops.h>
#include <kernel/log2.h>

#include "kconfig.h"

extern char _page_mem_start;
extern char _page_mem_end;

#if (PAGE_SIZE_SELECT == PAGE_SIZE_32K)

/* .pgmem section (32KB):
 *     - 128 pages of 256B require a  16-byte map
 *     -  64 pages of 512B require an  8-byte map
 *     -  32 pages of  1KB require a   4-byte map
 *     -  16 pages of  2KB require a   2-byte map
 *     -   8 pages of  4KB require a   1-byte map
 *
 * bit map = 0 means used (allocated) or undefined (i.e, not been allocated yet)
 * bit map = 1 means free (allocated) */
static unsigned long *const page_bitmap[] = {
    (unsigned long []){0, 0, 0, 0}, /* 16 bytes */
    (unsigned long []){0, 0},       /*  8 bytes */
    (unsigned long []){0},          /*  4 bytes */
    (unsigned long []){0},          /*  2 bytes */
    (unsigned long []){0xff},       /*  1 byte  */
};

static const unsigned long page_bitmap_sz[] = {128, 64, 32, 16, 8};
static const unsigned long page_order_sz[] = {256, 512, 1024, 2048, 4096};

#elif (PAGE_SIZE_SELECT == PAGE_SIZE_64K)

/* .pgmem section (64KB):
 *     - 256 pages of 256B require an 32-byte map
 *     - 128 pages of 512B require a  16-byte map
 *     -  64 pages of  1KB require a   8-byte map
 *     -  32 pages of  2KB require a   4-byte map
 *     -  16 pages of  4KB require a   2-byte map
 *     -   8 pages of  8KB require a   1-byte map
 *
 * bit map = 0 means used (allocated) or undefined (i.e, not been allocated yet)
 * bit map = 1 means free (allocated) */
static unsigned long *const page_bitmap[] = {
    (unsigned long []){0, 0, 0, 0, 0, 0, 0, 0}, /* 32 bytes */
    (unsigned long []){0, 0, 0, 0},             /* 16 bytes */
    (unsigned long []){0, 0},                   /*  8 bytes */
    (unsigned long []){0},                      /*  4 bytes */
    (unsigned long []){0},                      /*  2 bytes */
    (unsigned long []){0xff},                   /*  1 byte  */
};

static const unsigned long page_bitmap_sz[] = {256, 128, 64, 32, 16, 8};
static const unsigned long page_order_sz[] = {256, 512, 1024, 2048, 4096, 8192};

#endif

long size_to_page_order(unsigned long size)
{
    for(int i = 0; i <= PAGE_ORDER_MAX; i++) {
        if(size <= page_order_sz[i])
            return i;
    }

    return -1;
}

unsigned long page_order_to_size(long order)
{
    if(order > PAGE_ORDER_MAX)
        return 0;

    return page_order_sz[order];
}

unsigned long get_page_total_size(void)
{
    return (unsigned long)((uintptr_t)& _page_mem_end- (uintptr_t)&_page_mem_start);
}

static size_t order_to_page_size(int order)
{
    return page_order_sz[order];
}

unsigned long get_page_total_free_size(void)
{
    unsigned size = 0;

    /* iterate through all page orders */
    for(int i = 0; i <= PAGE_ORDER_MAX; i++) {
        int bit_num = 0;

        /* count the set bit of the bitmap */
        for(int j = 0; j < (page_bitmap_sz[i] / 8); j++)
            bit_num += __builtin_popcount(((uint8_t *)page_bitmap[i])[j]);

        /* use bit count to calculate the free memory */
        size += bit_num * order_to_page_size(i);
    }

    return size;
}

static long find_first_free_page(unsigned long order)
{
    /* find the first free page on the bitmap as its bit is marked
     * as 1 on the bitmap */
    unsigned long page_idx =
        find_first_bit(page_bitmap[order], page_bitmap_sz[order]);

    /* invalid page index number */
    if(page_idx >= page_bitmap_sz[order]) {
        return -1;
    }

    return page_idx;
}

static void split_first_free_page(unsigned long order)
{
    /* find index number of the first page with the given order */
    unsigned long page_idx =
        find_first_bit(page_bitmap[order], page_bitmap_sz[order]);

    /* clear bitmap of the first page in current order */
    bitmap_clear_bit(page_bitmap[order], page_idx);

    /* split the page into two pages and mark the bitmap of them in
     * smaller order */
    bitmap_set_bit(page_bitmap[order - 1], page_idx * 2);
    bitmap_set_bit(page_bitmap[order - 1], page_idx * 2 + 1);
}

static inline unsigned long get_buddy_index(unsigned long idx)
{
    return (idx % 2) ? (idx - 1) : (idx + 1);
}

static inline void *page_idx_to_addr(unsigned long idx, unsigned long order)
{
    /* page size = 2^(order + base)
     * address = page_start_address + index * page_size
     */
    unsigned long offset = idx * (1 << (order + ilog2(PAGE_SIZE_MIN)));
    return (void *)((uintptr_t)&_page_mem_start + offset);
}

static inline unsigned long addr_to_page_idx(unsigned long addr, unsigned long order)
{
    return (addr - (unsigned long)&_page_mem_start) >> (order + ilog2(PAGE_SIZE_MIN));
}

void *alloc_pages(unsigned long order)
{
    unsigned long page_idx, i;

    /* iterate from current order to higher order until a free page is found */
    for(i = order; (i <= PAGE_ORDER_MAX) && (find_first_free_page(i) < 0); i++);

    /* invalid order number */
    if(i > PAGE_ORDER_MAX)
        return NULL;

    /* split the found free page multiple times until the order
     * requirement is met */
    for(; i > order; i--)
        split_first_free_page(i);

    /* retrieve the free page to allocate by finding the first set bit on
     * the bitmap with respect to the order */
    page_idx = find_first_bit(page_bitmap[order], page_bitmap_sz[order]);

    /* clear the bit to indicate that the page is used */
    bitmap_clear_bit(page_bitmap[order], page_idx);

    /* return page address */
    return page_idx_to_addr(page_idx, order);
}

void free_pages(unsigned long addr, unsigned long order)
{
    unsigned long page_idx, buddy_idx, mask;

    /* attempt to coalesce pages from current order to the maximal order */
    for(; order < PAGE_ORDER_MAX; order++) {
        /* get indices of current page and buddy page */
        page_idx = addr_to_page_idx(addr, order);
        buddy_idx = get_buddy_index(page_idx);

        /* is the buddy page free now? (bitmap == 1) */
        if(bitmap_get_bit(page_bitmap[order], buddy_idx)) {
            /* yes, reset the bitmap to coalesce it */
            bitmap_clear_bit(page_bitmap[order], buddy_idx);

            /* generate all one's mask with respect to the order */
            mask = ~((1 << (order + 1 + ilog2(PAGE_SIZE_MIN))) - 1);
            addr &= mask;
        } else {
            /* set the page bitmap at this order and end the
             * coalescence. multiple pages are now coalesced
             * and free to use. */
            bitmap_set_bit(page_bitmap[order], page_idx);
            return;
        }
    }

    /* all lower order pages are now coalesced, set the bitmap map of
     * this big page to mark it as free to use */
    bitmap_set_bit(page_bitmap[order], addr_to_page_idx(addr, order));
}
