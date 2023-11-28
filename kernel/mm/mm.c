#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <arch/port.h>
#include <common/list.h>
#include <common/util.h>
#include <kernel/kernel.h>
#include <kernel/printk.h>
#include <kernel/syscall.h>
#include <kernel/thread.h>

#define MALLOC_BLK_FREE_MASK (1 << 30)
#define MALLOC_BLK_LEN_MASK (~(1 << 30))

extern char _user_stack_start;
extern char _user_stack_end;

struct malloc_info {
    /* Header */
    uint32_t block_info; /* [31]   - 0 as not free, 1 as free          *
                          * [30:0] - Block length including the header */
    struct list_head list;
    /* Data */
    char data[0];
};

LIST_HEAD(malloc_list);

static bool malloc_block_is_free(struct malloc_info *blk)
{
    return (blk->block_info & MALLOC_BLK_FREE_MASK) ? true : false;
}

static void malloc_set_block_free(struct malloc_info *blk, bool free)
{
    if (free)
        blk->block_info |= MALLOC_BLK_FREE_MASK;
    else
        blk->block_info &= MALLOC_BLK_LEN_MASK;
}

static size_t malloc_get_block_length(struct malloc_info *blk)
{
    return blk->block_info & MALLOC_BLK_LEN_MASK;
}

static void malloc_set_block_length(struct malloc_info *blk, size_t len)
{
    blk->block_info =
        (blk->block_info & MALLOC_BLK_FREE_MASK) | (len & MALLOC_BLK_LEN_MASK);
}

unsigned long heap_get_total_size(void)
{
    return (unsigned long) ((uintptr_t) &_user_stack_end -
                            (uintptr_t) &_user_stack_start);
}

unsigned long heap_get_free_size(void)
{
    unsigned long total_size = 0;

    /* Iterate through the whole malloc list */
    struct malloc_info *blk = NULL;
    list_for_each_entry (blk, &malloc_list, list) {
        /* Accumuate the size of all free pages */
        if (malloc_block_is_free(blk))
            total_size += malloc_get_block_length(blk);
    }

    return total_size;
}

void heap_init(void)
{
    /* Initialize the whole heap memory section as a free block */
    struct malloc_info *first_blk = (struct malloc_info *) &_user_stack_start;
    size_t len = (size_t) ((uintptr_t) &_user_stack_end -
                           (uintptr_t) &_user_stack_start);
    malloc_set_block_free(first_blk, true);
    malloc_set_block_length(first_blk, len);
    list_add(&first_blk->list, &malloc_list);
}

void *__malloc(size_t size)
{
    CURRENT_THREAD_INFO(curr_thread);

    /* Calculate the allocation size */
    size_t alloc_size = ALIGN(size + sizeof(struct malloc_info), sizeof(long));

    /* Iterate through the block list */
    struct malloc_info *blk = NULL;
    list_for_each_entry (blk, &malloc_list, list) {
        /* Check if the block is free or not */
        if (malloc_block_is_free(blk)) {
            /* Acquire the block length */
            size_t blk_len = malloc_get_block_length(blk);

            /* Find the first fit block */
            if (blk_len < alloc_size) {
                /* The free block is not large enough */
                continue;
            } else if (blk_len < alloc_size + sizeof(struct malloc_info)) {
                /* A free block is found but can not be further splitted */
                malloc_set_block_free(blk, false);

                /* Return the memory address */
                return blk->data;
            } else {
                /* A free block is found, which not only has enough size
                 * but also can be further splitted */

                /* Split the free block by schrinking the size */
                malloc_set_block_length(
                    blk, malloc_get_block_length(blk) - alloc_size);

                /* The splitted part are now ready to use */
                struct malloc_info *new_blk =
                    (struct malloc_info *) ((uintptr_t) blk +
                                            malloc_get_block_length(blk));
                malloc_set_block_free(new_blk, false);
                malloc_set_block_length(new_blk, alloc_size);
                list_add(&new_blk->list, &malloc_list);

                /* Return the memory address */
                return new_blk->data;
            }
        }
    }

    /* Failed to allocate memory */
    printk("malloc(): not enough heap space (name: %s, pid: %d)",
           curr_thread->name, curr_thread->task->pid);

    return NULL;
}

NACKED void *malloc(size_t size)
{
    SYSCALL(MALLOC);
}

void __free(void *ptr)
{
    struct malloc_info *curr_blk = container_of(ptr, struct malloc_info, data);
    struct malloc_info *prev_blk, *next_blk;

    /* Free the current block */
    malloc_set_block_free(curr_blk, true);

    /* Check if previous block exists */
    if (curr_blk->list.prev != &malloc_list) {
        prev_blk = list_prev_entry(curr_blk, list);

        /* Merge the previous block if it is free */
        if (malloc_block_is_free(prev_blk)) {
            size_t len = malloc_get_block_length(prev_blk) +
                         malloc_get_block_length(curr_blk);
            malloc_set_block_length(prev_blk, len);
            list_del(&curr_blk->list);
            curr_blk = prev_blk;
        }
    }

    /* Check if next block exists */
    if (curr_blk->list.next != &malloc_list) {
        next_blk = list_next_entry(curr_blk, list);

        /* Merge the next block if it is free */
        if (malloc_block_is_free(next_blk)) {
            size_t len = malloc_get_block_length(curr_blk) +
                         malloc_get_block_length(next_blk);
            malloc_set_block_length(curr_blk, len);
            list_del(&next_blk->list);
        }
    }
}

NACKED void free(void *ptr)
{
    SYSCALL(FREE);
}

void *calloc(size_t nmemb, size_t size)
{
    /* Calculate the allocation size and detect the overflow */
    size_t total_size;
    if (__builtin_mul_overflow(nmemb, size, &total_size))
        return NULL;

    /* Allocate new memory */
    void *mem = malloc(total_size);
    if (!mem)
        return NULL;

    /* Reset the memory data */
    memset(mem, 0, nmemb * size);
    return mem;
}

NACKED int minfo(int name)
{
    SYSCALL(MINFO);
}

/* Not implemented. The function is defined only
 * to supress the newlib warning.
 */
void *_sbrk(int incr)
{
    return NULL;
}
