#include <errno.h>
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
    list_add_tail(&first_blk->list, &malloc_list);
}

void *__malloc(size_t size)
{
    CURRENT_THREAD_INFO(curr_thread);

    /* Calculate the allocation size. The size must be rounded up, otherwise
     * the caller gets less memory than it asked for.
     */
    size_t alloc_size =
        ALIGN_UP(size + sizeof(struct malloc_info), sizeof(long));

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

                /* The splitted part sits right behind the block it came from.
                 * __free() merges with the list neighbors of a block, so the
                 * list has to stay sorted by address, or blocks that are not
                 * adjacent in memory end up being merged.
                 */
                list_add(&new_blk->list, &blk->list);

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

/* An aligned block is carved out of a plain one: the memory comes from
 * __malloc() with room to spare, and the part in front of the address that
 * answers is handed back as a free block of its own, so that __free() takes
 * the result the same way it takes any other
 */
void *__memalign(size_t alignment, size_t size)
{
    /* Every allocation already comes back on a boundary this wide */
    if (alignment <= sizeof(long))
        return __malloc(size);

    /* Room for the address to move up to a boundary, and for what is left in
     * front of it to be long enough to be a block
     */
    void *ptr = __malloc(size + alignment + sizeof(struct malloc_info));
    if (!ptr)
        return NULL;

    uintptr_t aligned = ALIGN_UP((uintptr_t) ptr, alignment);
    if (aligned == (uintptr_t) ptr)
        return ptr;

    /* What is handed back in front has to hold a header, so the address moves
     * up to the first boundary that leaves room for one
     */
    aligned = ALIGN_UP((uintptr_t) ptr + sizeof(struct malloc_info), alignment);

    struct malloc_info *blk = container_of(ptr, struct malloc_info, data);
    struct malloc_info *aligned_blk =
        container_of((char *) aligned, struct malloc_info, data);

    size_t front_len = (uintptr_t) aligned_blk - (uintptr_t) blk;
    size_t blk_len = malloc_get_block_length(blk);

    malloc_set_block_length(aligned_blk, blk_len - front_len);
    malloc_set_block_free(aligned_blk, false);

    /* The block in front sits before this one in memory, and __free() merges a
     * block with its list neighbours, so the list has to stay sorted by address
     */
    list_add(&aligned_blk->list, &blk->list);

    /* What is left in front is given back */
    malloc_set_block_length(blk, front_len);
    malloc_set_block_free(blk, true);

    return aligned_blk->data;
}

NACKED void *malloc(size_t size)
{
    SYSCALL(MALLOC);
}

static NACKED void *__sys_memalign(size_t alignment, size_t size)
{
    SYSCALL(MEMALIGN);
}

int posix_memalign(void **memptr, size_t alignment, size_t size)
{
    /* POSIX asks for a power of two that a pointer fits in */
    if (alignment < sizeof(void *) || (alignment & (alignment - 1)))
        return EINVAL;

    void *ptr = __sys_memalign(alignment, size);
    if (!ptr && size)
        return ENOMEM;

    *memptr = ptr;

    return 0;
}

void __free(void *ptr)
{
    /* Freeing a null pointer does nothing, which the C standard requires and
     * which callers rely on: "free(p)" where p may or may not have been
     * allocated is an ordinary thing to write.
     */
    if (!ptr)
        return;

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

void *__realloc(void *ptr, size_t size)
{
    /* Growing nothing is an allocation and shrinking to nothing is a free,
     * both of which the C standard asks for
     */
    if (!ptr)
        return __malloc(size);
    if (size == 0) {
        __free(ptr);
        return NULL;
    }

    /* The header of the block already records how much room it has, so the
     * old size is there to be read rather than to be remembered
     */
    struct malloc_info *blk = container_of(ptr, struct malloc_info, data);
    size_t old_size = malloc_get_block_length(blk) - sizeof(struct malloc_info);

    /* The block the caller already holds is large enough. Handing it back
     * keeps the contents where they are and costs nothing
     */
    if (old_size >= size)
        return ptr;

    /* The block has to move. The old contents come along, which is what
     * separates realloc() from a free() followed by a malloc()
     */
    void *new_ptr = __malloc(size);
    if (!new_ptr)
        return NULL;

    memcpy(new_ptr, ptr, old_size);
    __free(ptr);

    return new_ptr;
}

NACKED void *realloc(void *ptr, size_t size)
{
    SYSCALL(REALLOC);
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

/* The duplicators of the C library allocate, so they belong next to the
 * allocator: the ones newlib provides take the memory from its own heap
 */
char *strdup(const char *s)
{
    size_t size = strlen(s) + 1;
    char *copy = malloc(size);

    if (!copy) {
        errno = ENOMEM;
        return NULL;
    }

    memcpy(copy, s, size);

    return copy;
}

char *strndup(const char *s, size_t n)
{
    size_t len = strnlen(s, n);
    char *copy = malloc(len + 1);

    if (!copy) {
        errno = ENOMEM;
        return NULL;
    }

    memcpy(copy, s, len);
    copy[len] = '\0';

    return copy;
}

NACKED int minfo(int name)
{
    SYSCALL(MINFO);
}

/* Tenok has no break to move. Answering with the value the C library reads as
 * a failure keeps its allocator from handing out addresses near zero
 */
void *_sbrk(int incr)
{
    return (void *) -1;
}
